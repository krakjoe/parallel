/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2024                                  |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PARALLEL_SCHEDULER
#define HAVE_PARALLEL_SCHEDULER

#include "../php_parallel.h"
#include "parallel.h"
#include "zend_types.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

TSRM_TLS php_parallel_runtime_t *php_parallel_scheduler_context = NULL;
TSRM_TLS php_parallel_future_t  *php_parallel_scheduler_future = NULL;

void (*zend_interrupt_handler)(zend_execute_data *) = NULL;

/* The only crash we deliberately convert into a catchable error: a parallel
 * worker faulting on a call to a function that does not exist in the worker's
 * function table (e.g. not provided via a bootstrap file). On success it
 * records the diagnostics on the runtime and returns true so the caller can
 * zend_bailout(). Every other crash is a genuine bug and must reach the
 * previously installed handler, so this returns false and stashes nothing. */
static bool php_parallel_scheduler_recover_missing_function(void)
{
	php_parallel_runtime_t *runtime = php_parallel_scheduler_context;

	if (!runtime || !EG(current_execute_data) || !EG(current_execute_data)->opline) {
		return false;
	}

	const zend_op *opline = EG(current_execute_data)->opline;

	if (opline->opcode != ZEND_INIT_FCALL) {
		return false;
	}

	zval *name = RT_CONSTANT(opline, opline->op2);

	if (Z_TYPE_P(name) != IS_STRING || zend_hash_exists(EG(function_table), Z_STR_P(name))) {
		return false;
	}

	runtime->crashed = 1;
	runtime->missing = Z_STR_P(name);
	runtime->line = opline->lineno;

	if (EG(current_execute_data)->func && EG(current_execute_data)->func->op_array.filename) {
		runtime->file = EG(current_execute_data)->func->op_array.filename;
	}

	return true;
}

#ifdef _WIN32
void       *php_parallel_veh_handle = NULL;

LONG WINAPI php_parallel_veh_handler(PEXCEPTION_POINTERS exceptionInfo)
{
	if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		// Only convert into a catchable error when a parallel worker crashed
		// calling an undefined function. Any other access violation is a genuine
		// bug: forward it to the previously installed handler (and crash cleanly
		// if nothing handles it) rather than masking it.
		if (php_parallel_scheduler_recover_missing_function()) {
			zend_bailout();
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}
#else
struct sigaction php_parallel_old_sigsegv_action;

static void      php_parallel_sigsegv_handler(int sig, siginfo_t *info, void *context)
{
	// Only convert into a catchable error when a parallel worker crashed
	// calling an undefined function. Any other crash is a genuine bug and must
	// reach the previous handler so it produces a backtrace / core dump.
	if (php_parallel_scheduler_recover_missing_function()) {
		zend_bailout();
	}

	if (php_parallel_old_sigsegv_action.sa_flags & SA_SIGINFO) {
		php_parallel_old_sigsegv_action.sa_sigaction(sig, info, context);
	} else if (php_parallel_old_sigsegv_action.sa_handler == SIG_DFL) {
		signal(sig, SIG_DFL);
		raise(sig);
	} else if (php_parallel_old_sigsegv_action.sa_handler == SIG_IGN) {
		// ignore
	} else {
		php_parallel_old_sigsegv_action.sa_handler(sig);
	}
}
#endif

static zend_always_inline int  php_parallel_scheduler_list_delete(void *lhs, void *rhs) { return lhs == rhs; }

static zend_always_inline bool php_parallel_scheduler_exit_exception(void)
{
#if PHP_VERSION_ID >= 80400
	zend_object *exception = EG(exception);

	/* PHP 8.4 models exit as an internal exception that must not be copied as a task error. */
	return exception && (zend_is_unwind_exit(exception) || zend_is_graceful_exit(exception));
#else
	return false;
#endif
}

static zend_always_inline void php_parallel_scheduler_kill_future(php_parallel_future_t *future)
{
	php_parallel_monitor_lock(future->monitor);
	if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_CANCELLED)) {
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_KILLED);
	}
	php_parallel_monitor_unlock(future->monitor);
}

static void php_parallel_schedule_free_function(zend_function *function)
{
	if (function->op_array.static_variables) {
		php_parallel_copy_hash_dtor(function->op_array.static_variables, 1);
		ZEND_MAP_PTR_SET(function->op_array.static_variables_ptr, NULL);
		function->op_array.static_variables = NULL;
	}

#if PHP_VERSION_ID >= 80100
	if (function->op_array.num_dynamic_func_defs) {
		uint32_t it = 0;

		while (it < function->op_array.num_dynamic_func_defs) {
			php_parallel_schedule_free_function((zend_function *)function->op_array.dynamic_func_defs[it]);
			it++;
		}
	}
#endif
}

static void php_parallel_schedule_free(void *scheduleed)
{
	php_parallel_schedule_el_t *el = (php_parallel_schedule_el_t *)scheduleed;
	zval                       *slot = (zval *)ZEND_CALL_ARG(el->frame, 1), *end = slot + ZEND_CALL_NUM_ARGS(el->frame);

	while (slot < end) {
		PARALLEL_ZVAL_DTOR(slot);
		slot++;
	}

	if (el->frame->func) {
		php_parallel_schedule_free_function(el->frame->func);
		pefree(el->frame->func, 1);
	}

	pefree(el->frame, 1);
}

void php_parallel_scheduler_init(php_parallel_runtime_t *runtime)
{
	zend_llist_init(&runtime->schedule, sizeof(php_parallel_schedule_el_t), php_parallel_schedule_free, 1);
}

void php_parallel_scheduler_destroy(php_parallel_runtime_t *runtime) { zend_llist_destroy(&runtime->schedule); }

static zend_always_inline php_parallel_runtime_t *php_parallel_scheduler_setup(php_parallel_runtime_t *runtime)
{
	php_parallel_scheduler_context = runtime;

	ts_resource(0);

	TSRMLS_CACHE_UPDATE();

	SG(server_context) = runtime->parent.server;
	SG(request_info).argc = runtime->parent.argc;
	SG(request_info).argv = runtime->parent.argv;

	runtime->child.interrupt = &EG(vm_interrupt);

	PG(expose_php) = 0;
	PG(auto_globals_jit) = 1;
#if PHP_VERSION_ID >= 80100
	PG(enable_dl) = false;
#else
	PG(enable_dl) = 0;
#endif

	php_request_startup();

	PG(during_request_startup) = 0;
	SG(sapi_started) = 0;
	SG(headers_sent) = 1;
	SG(request_info).no_headers = 1;

	return runtime;
}

static zend_always_inline void php_parallel_scheduler_exit(php_parallel_runtime_t *runtime)
{
	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_DONE);

	/*
	 * Force cycle collection before shutdown to clean up self-referential
	 * closures created inside the thread (e.g., recursive closures using
	 * `use (&$self)`). These create cycles that won't be freed by normal
	 * refcount decrement.
	 */
	gc_collect_cycles();

	php_request_shutdown(NULL);

	ts_free_thread();

	pthread_exit(NULL);
}

static zend_always_inline void php_parallel_scheduler_add(php_parallel_runtime_t *runtime,
                                                          const zend_function *function, zval *argv,
                                                          php_parallel_future_t *future)
{
	php_parallel_schedule_el_t el;
	uint32_t                   argc = argv && Z_TYPE_P(argv) == IS_ARRAY ? zend_hash_num_elements(Z_ARRVAL_P(argv)) : 0;

	zend_execute_data         *frame =
	    (zend_execute_data *)pecalloc(1, zend_vm_calc_used_stack(argc, (zend_function *)function), 1);

	frame->func = php_parallel_cache_closure(function, NULL);

	php_parallel_dependencies_store(frame->func);

	if (argv && Z_TYPE_P(argv) == IS_ARRAY) {
		zval                        *slot = ZEND_CALL_ARG(frame, 1);
		zval                        *param;
		uint32_t                     argc = 0;
		php_parallel_copy_context_t *context, *restore;

		context = php_parallel_copy_context_start(PHP_PARALLEL_COPY_DIRECTION_PERSISTENT, &restore);

		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(argv), param)
		{
			PARALLEL_ZVAL_COPY(slot, param, 1);
			slot++;
			argc++;
		}
		ZEND_HASH_FOREACH_END();

		ZEND_CALL_NUM_ARGS(frame) = argc;

		php_parallel_copy_context_end(context, restore);
	} else {
		ZEND_CALL_NUM_ARGS(frame) = 0;
	}

	if (future) {
		frame->return_value = &future->value;
		Z_PTR(frame->This) = future;
	}

	el.frame = frame;

	zend_llist_add_element(&runtime->schedule, &el);

	if (future) {
		future->runtime = runtime;
		future->handle = runtime->schedule.tail->data;
		GC_ADDREF(&runtime->std);
	}
}

static zend_always_inline bool php_parallel_scheduler_empty(php_parallel_runtime_t *runtime)
{
	return !zend_llist_count(&runtime->schedule);
}

bool php_parallel_scheduler_busy(php_parallel_runtime_t *runtime)
{
	bool busy = 1;

	php_parallel_monitor_lock(runtime->monitor);
	if (php_parallel_scheduler_empty(runtime)) {
		if (!php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_RUNNING)) {
			busy = 0;
		}
	}
	php_parallel_monitor_unlock(runtime->monitor);

	return busy;
}

static void php_parallel_scheduler_pull(zend_function *function)
{
	if (function->op_array.static_variables) {
		HashTable *statics = function->op_array.static_variables;

		function->op_array.static_variables = php_parallel_copy_hash_ctor(statics, 0);

#if PHP_VERSION_ID >= 80200
		ZEND_MAP_PTR_INIT(function->op_array.static_variables_ptr, function->op_array.static_variables);
#else
		ZEND_MAP_PTR_INIT(function->op_array.static_variables_ptr, &function->op_array.static_variables);
#endif

		if (GC_FLAGS(statics) & IS_ARRAY_PERSISTENT) {
			php_parallel_copy_hash_dtor(statics, 1);
		}
	}

#if PHP_VERSION_ID < 80200
	ZEND_MAP_PTR_NEW(function->op_array.run_time_cache);
#else
	ZEND_MAP_PTR_INIT(function->op_array.run_time_cache, NULL);
#endif

#if PHP_VERSION_ID >= 80100
	if (function->op_array.num_dynamic_func_defs) {
		uint32_t it = 0;

		while (it < function->op_array.num_dynamic_func_defs) {
			php_parallel_scheduler_pull((zend_function *)function->op_array.dynamic_func_defs[it]);
			it++;
		}
	}
#endif
}

static void php_parallel_scheduler_clean(zend_function *function)
{
	if (function->op_array.static_variables) {
		HashTable *statics = ZEND_MAP_PTR_GET(function->op_array.static_variables_ptr);

		if (!(GC_FLAGS(statics) & IS_ARRAY_IMMUTABLE)) {
			zend_array_destroy(statics);
			ZEND_MAP_PTR_SET(function->op_array.static_variables_ptr, NULL);
			function->op_array.static_variables = NULL;
		}
	}

#if PHP_VERSION_ID >= 80100
	if (function->op_array.num_dynamic_func_defs) {
		uint32_t it = 0;

		while (it < function->op_array.num_dynamic_func_defs) {
			php_parallel_scheduler_clean((zend_function *)function->op_array.dynamic_func_defs[it]);
			pefree(function->op_array.dynamic_func_defs[it], 1);
			it++;
		}
		/* Free the dynamic_func_defs array itself */
		pefree(function->op_array.dynamic_func_defs, 1);
		function->op_array.dynamic_func_defs = NULL;
	}
#endif
}

static zend_always_inline bool php_parallel_scheduler_pop(php_parallel_runtime_t     *runtime,
                                                          php_parallel_schedule_el_t *el)
{
	php_parallel_schedule_el_t *head;
	zend_class_entry           *scope = NULL;
	zend_function              *function = NULL;

	if (zend_llist_count(&runtime->schedule) == 0) {
		return 0;
	}

	head = zend_llist_get_first(&runtime->schedule);
	function = head->frame->func;
	head->frame->func = NULL;

	if (function->op_array.scope && function->op_array.scope->type == ZEND_USER_CLASS) {
		scope = php_parallel_copy_scope(function->op_array.scope);
	}

	el->frame = zend_vm_stack_push_call_frame(ZEND_CALL_TOP_FUNCTION, php_parallel_copy_function(function, 0),
	                                          ZEND_CALL_NUM_ARGS(head->frame), scope);

	if (scope != function->op_array.scope) {
		el->frame->func->op_array.scope = scope;
	}

	php_parallel_scheduler_pull(el->frame->func);

	if (ZEND_CALL_NUM_ARGS(head->frame)) {
		zval *slot = (zval *)ZEND_CALL_ARG(head->frame, 1), *end = slot + ZEND_CALL_NUM_ARGS(head->frame);
		zval *param = ZEND_CALL_ARG(el->frame, 1);

		php_parallel_copy_context_t *context, *restore;

		context = php_parallel_copy_context_start(PHP_PARALLEL_COPY_DIRECTION_THREAD, &restore);

		while (slot < end) {
			PARALLEL_ZVAL_COPY(param, slot, 0);
			slot++;
			param++;
		}

		php_parallel_copy_context_end(context, restore);
	}

	zend_init_func_execute_data(el->frame, &el->frame->func->op_array, head->frame->return_value);

	php_parallel_scheduler_future = Z_PTR(head->frame->This);

	zend_llist_del_element(&runtime->schedule, head, php_parallel_scheduler_list_delete);

	return 1;
}

static void php_parallel_scheduler_run(php_parallel_runtime_t *runtime, zend_execute_data *frame)
{
	php_parallel_future_t *future = php_parallel_scheduler_future;

	/* READY allows the consumer to destroy the future, so publish it only after
	 * the worker's final access through frame->return_value. */
	volatile int32_t       future_state = PHP_PARALLEL_READY;

	runtime->crashed = 0;
	runtime->missing = NULL;
	runtime->file = NULL;
	runtime->line = 0;

	zend_first_try
	{
		zend_try
		{
			zend_execute_ex(frame);

			if (UNEXPECTED(EG(exception))) {
				if (future) {
					if (php_parallel_scheduler_exit_exception()) {
						zend_clear_exception();

						php_parallel_scheduler_kill_future(future);
					} else {
						php_parallel_exceptions_save(frame->return_value, EG(exception));

						future_state |= PHP_PARALLEL_ERROR;
					}
				} else {
					zend_throw_exception_internal(NULL);
				}
			}
		}
		zend_catch
		{
			if (runtime->crashed) {
				zend_object *exception = NULL;
				if (future) {
					char *message = NULL;

					zend_clear_exception();

					if (runtime->missing) {
						spprintf(
						    &message, 0,
						    "Call to undefined function %s() in task, please provide the function via a bootstrap file",
						    ZSTR_VAL(runtime->missing));
						exception = php_parallel_exception_object(php_parallel_runtime_error_illegal_instruction_ce,
						                                          message, 0, runtime->file, runtime->line);
						efree(message);
					} else {
						exception = php_parallel_exception_object(php_parallel_runtime_error_illegal_instruction_ce,
						                                          "illegal instruction (segmentation fault) in task", 0,
						                                          runtime->file, runtime->line);
					}

					php_parallel_monitor_lock(future->monitor);
					if (exception) {
						php_parallel_exceptions_save(frame->return_value, exception);
						OBJ_RELEASE(exception);
					}
					php_parallel_monitor_unlock(future->monitor);

					future_state |= PHP_PARALLEL_ERROR;
				}

				// This runtime crashed, as such we can not give any guarantees
				// anymore and it is best to shut down the thread as gracefuly
				// as possible. By setting the `runtime->monitor` to
				// `PHP_PARALLEL_KILLED` we make sure that this thread goes
				// through shutdown and calls `pthread_ext()`
				php_parallel_monitor_lock(runtime->monitor);
				php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_DONE | PHP_PARALLEL_KILLED);
				php_parallel_monitor_unlock(runtime->monitor);
			} else if (future) {
				php_parallel_scheduler_kill_future(future);
			}
		}
		zend_end_try();

		if (frame->return_value && !Z_ISUNDEF_P(frame->return_value)) {
			zval garbage = *frame->return_value;

			if (Z_OPT_REFCOUNTED(garbage)) {
				PARALLEL_ZVAL_COPY(frame->return_value, &garbage, 1);

				zval_ptr_dtor(&garbage);
			}
		}

		php_parallel_scheduler_clean(frame->func);

		pefree(frame->func, 1);

		zend_vm_stack_free_call_frame(frame);
	}
	zend_end_try();

	if (future) {
		php_parallel_monitor_set(future->monitor, future_state);
	}

	php_parallel_scheduler_future = NULL;
}

static void php_parallel_scheduler_killed(void *scheduled)
{
	php_parallel_schedule_el_t *el = (php_parallel_schedule_el_t *)scheduled;
	php_parallel_future_t      *future = Z_PTR(el->frame->This);

	if (future) {
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_READY | PHP_PARALLEL_KILLED);
	}
}

static zend_always_inline void php_parallel_scheduler_kill(php_parallel_runtime_t *runtime)
{
	zend_llist_apply(&runtime->schedule, php_parallel_scheduler_killed);
	zend_llist_clean(&runtime->schedule);
}

static zend_always_inline int php_parallel_thread_bootstrap(zend_string *file)
{
	zend_file_handle fh;
	zend_op_array   *ops;
	zval             rv;
	int              result;

	if (!file) {
		return SUCCESS;
	}

#if PHP_VERSION_ID >= 80100
	zend_stream_init_filename_ex(&fh, file);

	result = php_stream_open_for_zend_ex(&fh, USE_PATH | REPORT_ERRORS | STREAM_OPEN_FOR_INCLUDE);
#else
	result = php_stream_open_for_zend_ex(ZSTR_VAL(file), &fh, USE_PATH | REPORT_ERRORS | STREAM_OPEN_FOR_INCLUDE);
#endif

	if (result != SUCCESS) {
		return FAILURE;
	}

	zend_hash_add_empty_element(&EG(included_files), fh.opened_path ? fh.opened_path : file);

	ops = zend_compile_file(&fh, ZEND_REQUIRE);

	zend_destroy_file_handle(&fh);

	if (ops) {
		ZVAL_UNDEF(&rv);
		zend_execute(ops, &rv);
		destroy_op_array(ops);
		efree(ops);

		if (EG(exception)) {
			zend_clear_exception();
			return FAILURE;
		}

		zval_ptr_dtor(&rv);
		return SUCCESS;
	}

	if (EG(exception)) {
		zend_clear_exception();
	}

	return FAILURE;
}

// Implements the thread main loop. This bootstraps the thread by including the
// bootstrap PHP file in case one was specified and afterwards set the runtimes
// monitor to ready and running (and with this unblocking the calling
// `php_parallel_scheduler_start()` function).
static void *php_parallel_thread(void *arg)
{
	int32_t                 state = 0;

	php_parallel_runtime_t *runtime = php_parallel_scheduler_setup((php_parallel_runtime_t *)arg);

	if (php_parallel_thread_bootstrap(runtime->bootstrap) != SUCCESS) {
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_FAILURE);

		goto _php_parallel_thread_exit;
	}

	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_READY | PHP_PARALLEL_RUNNING);

	do {
		php_parallel_schedule_el_t el;

		if (php_parallel_monitor_lock(runtime->monitor) != SUCCESS) {
			break;
		}

		if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_KILLED)) {
		_php_parallel_thread_killed:
			php_parallel_scheduler_kill(runtime);

			php_parallel_monitor_unlock(runtime->monitor);
			goto _php_parallel_thread_exit;
		}

		while (!php_parallel_scheduler_pop(runtime, &el)) {
			php_parallel_monitor_remove(runtime->monitor, PHP_PARALLEL_RUNNING);

			if (!(state & PHP_PARALLEL_CLOSE)) {
				state = php_parallel_monitor_wait_locked(runtime->monitor,
				                                         PHP_PARALLEL_EXEC | PHP_PARALLEL_CLOSE | PHP_PARALLEL_KILLED);
			}

			if ((state & (PHP_PARALLEL_CLOSE | PHP_PARALLEL_KILLED))) {
				if ((state & PHP_PARALLEL_KILLED)) {
					goto _php_parallel_thread_killed;
				}

				if (php_parallel_scheduler_empty(runtime)) {
					php_parallel_monitor_unlock(runtime->monitor);
					goto _php_parallel_thread_exit;
				}
			}
		}

		php_parallel_monitor_add(runtime->monitor, PHP_PARALLEL_RUNNING);

		php_parallel_monitor_unlock(runtime->monitor);

		php_parallel_scheduler_run(runtime, el.frame);
	} while (1);

_php_parallel_thread_exit:
	php_parallel_scheduler_exit(runtime);

	return NULL;
}

// Creates a monitor for the `runtime` and spawns a new thread. After spawning
// blocks until the newly created thread enters either the ready or the failure
// state. Throws a userland exception in case the thread creation failed.
void php_parallel_scheduler_start(php_parallel_runtime_t *runtime, zend_string *bootstrap)
{
	uint32_t state = SUCCESS;

	if (bootstrap) {
		runtime->bootstrap = php_parallel_copy_string_interned(bootstrap);
	}

	runtime->monitor = php_parallel_monitor_create();

	if (pthread_create(&runtime->thread, NULL, php_parallel_thread, runtime) != SUCCESS) {
		php_parallel_exception_ex(php_parallel_runtime_error_ce, "cannot create thread %s", strerror(errno));
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_FAILURE);
		php_parallel_scheduler_stop(runtime);
		return;
	}

	state = php_parallel_monitor_wait(runtime->monitor, PHP_PARALLEL_READY | PHP_PARALLEL_FAILURE);

	if ((state == FAILURE) || (state & PHP_PARALLEL_FAILURE)) {
		php_parallel_exception_ex(php_parallel_runtime_error_bootstrap_ce, "bootstrapping failed with %s",
		                          ZSTR_VAL(runtime->bootstrap));
		php_parallel_scheduler_stop(runtime);
	}
}

void php_parallel_scheduler_stop(php_parallel_runtime_t *runtime)
{
	if (!runtime->monitor) {
		return;
	}

	php_parallel_monitor_lock(runtime->monitor);

	if (!php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_CLOSE);

		if (!php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_FAILURE)) {
			php_parallel_monitor_wait_locked(runtime->monitor, PHP_PARALLEL_DONE);
		}

		php_parallel_monitor_unlock(runtime->monitor);

		pthread_join(runtime->thread, NULL);
	} else {
		php_parallel_monitor_unlock(runtime->monitor);
	}

	php_parallel_monitor_destroy(runtime->monitor);

	runtime->monitor = NULL;
}

/// Adds the task in `closure` to the thread referenced by `runtime`. In case
/// the task returns anything it also creates the future to return.
void php_parallel_scheduler_push(php_parallel_runtime_t *runtime, zval *closure, zval *argv, zval *return_value)
{
	zend_execute_data     *caller = EG(current_execute_data)->prev_execute_data;
	const zend_function   *function = zend_get_closure_method_def(Z_OBJ_P(closure));
	bool                   returns = 0;
	php_parallel_future_t *future = NULL;

	php_parallel_monitor_lock(runtime->monitor);

	if (!php_parallel_check_task(runtime, caller, function, argv, &returns)) {
		php_parallel_monitor_unlock(runtime->monitor);
		return;
	}

	if (returns) {
		future = php_parallel_future_construct(return_value);
	}

	php_parallel_scheduler_add(runtime, function, argv, future);

	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_EXEC);
	php_parallel_monitor_unlock(runtime->monitor);
}

void php_parallel_scheduler_join(php_parallel_runtime_t *runtime, bool kill)
{
	php_parallel_monitor_lock(runtime->monitor);

	if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
		php_parallel_exception_ex(php_parallel_runtime_error_closed_ce, "Runtime closed");
		php_parallel_monitor_unlock(runtime->monitor);
		return;
	}

	if (kill) {
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_KILLED);

		zend_atomic_bool_store(runtime->child.interrupt, true);
	} else {
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_CLOSE);
	}

	php_parallel_monitor_wait_locked(runtime->monitor, PHP_PARALLEL_DONE);

	php_parallel_monitor_unlock(runtime->monitor);

	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_CLOSED);

	pthread_join(runtime->thread, NULL);
}

bool php_parallel_scheduler_cancel(php_parallel_future_t *future)
{
	size_t in, out = 0;

	php_parallel_monitor_lock(future->runtime->monitor);

	in = zend_llist_count(&future->runtime->schedule);

	zend_llist_del_element(&future->runtime->schedule, future->handle, php_parallel_scheduler_list_delete);

	out = zend_llist_count(&future->runtime->schedule);

	php_parallel_monitor_unlock(future->runtime->monitor);

	if (in == out) {
		bool cancelled = 0;

		php_parallel_monitor_lock(future->monitor);

		if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY)) {
			zend_atomic_bool_store(future->runtime->child.interrupt, true);

			php_parallel_monitor_set(future->monitor, PHP_PARALLEL_CANCELLED);
			php_parallel_monitor_wait_locked(future->monitor, PHP_PARALLEL_READY);
			php_parallel_monitor_set(future->monitor, PHP_PARALLEL_READY | PHP_PARALLEL_DONE);

			cancelled = 1;
		}

		php_parallel_monitor_unlock(future->monitor);

		return cancelled;
	} else {
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_READY | PHP_PARALLEL_DONE | PHP_PARALLEL_CANCELLED);

		return 1;
	}
}

PARALLEL_API bool php_parallel_is_parallel_worker_thread(void) { return php_parallel_scheduler_context != NULL; }

static void       php_parallel_scheduler_interrupt(zend_execute_data *execute_data)
{
	if (php_parallel_scheduler_context) {
		php_parallel_monitor_lock(php_parallel_scheduler_context->monitor);
		if (php_parallel_monitor_check(php_parallel_scheduler_context->monitor, PHP_PARALLEL_KILLED)) {
			php_parallel_monitor_unlock(php_parallel_scheduler_context->monitor);
			zend_bailout();
		}
		php_parallel_monitor_unlock(php_parallel_scheduler_context->monitor);

		if (php_parallel_scheduler_future) {
			php_parallel_monitor_lock(php_parallel_scheduler_future->monitor);
			if (php_parallel_monitor_check(php_parallel_scheduler_future->monitor, PHP_PARALLEL_CANCELLED)) {
				php_parallel_monitor_unlock(php_parallel_scheduler_future->monitor);
				zend_bailout();
			}
			php_parallel_monitor_unlock(php_parallel_scheduler_future->monitor);
		}
	}

	if (zend_interrupt_handler) {
		zend_interrupt_handler(execute_data);
	}
}

PHP_MINIT_FUNCTION(PARALLEL_SCHEDULER)
{
	zend_interrupt_handler = zend_interrupt_function;
	zend_interrupt_function = php_parallel_scheduler_interrupt;

#ifdef _WIN32
	php_parallel_veh_handle = AddVectoredExceptionHandler(1, php_parallel_veh_handler);
#else
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = php_parallel_sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGSEGV, &sa, &php_parallel_old_sigsegv_action);
#endif

	PHP_MINIT(PARALLEL_RUNTIME)(INIT_FUNC_ARGS_PASSTHRU);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_SCHEDULER)
{
#ifdef _WIN32
	if (php_parallel_veh_handle) {
		RemoveVectoredExceptionHandler(php_parallel_veh_handle);
	}
#else
	sigaction(SIGSEGV, &php_parallel_old_sigsegv_action, NULL);
#endif

	zend_interrupt_function = zend_interrupt_handler;

	PHP_MSHUTDOWN(PARALLEL_RUNTIME)(INIT_FUNC_ARGS_PASSTHRU);

	return SUCCESS;
}
#endif
