/*
  +----------------------------------------------------------------------+
  | parallel                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019                                       |
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
#ifndef HAVE_PARALLEL_RUNTIME
#define HAVE_PARALLEL_RUNTIME

#include "parallel.h"

zend_class_entry *php_parallel_runtime_ce;
zend_object_handlers php_parallel_runtime_handlers;

zend_string *php_parallel_runtime_main;

static zend_always_inline int php_parallel_runtime_bootstrap(zend_string *file) {
	zend_file_handle fh;
	zend_op_array *ops;
	zval rv;
	int result;
	
	if (!file) {
	    return SUCCESS;
	}
	
	result = php_stream_open_for_zend_ex(ZSTR_VAL(file), &fh, USE_PATH|REPORT_ERRORS|STREAM_OPEN_FOR_INCLUDE);

	if (result != SUCCESS) {
		return FAILURE;
	}

	if (!fh.opened_path) {
		fh.opened_path = zend_string_dup(file, 0);
	}

	zend_hash_add_empty_element(&EG(included_files), fh.opened_path);

	ops = zend_compile_file(
		&fh, ZEND_REQUIRE);
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

static void* php_parallel_runtime(void *arg) {	
	int32_t state = 0;

	php_parallel_runtime_t *runtime = 
	    php_parallel_scheduler_setup(
	        (php_parallel_runtime_t*) arg);

	if (php_parallel_runtime_bootstrap(runtime->bootstrap) != SUCCESS) {
		php_parallel_monitor_set(
			runtime->monitor, PHP_PARALLEL_FAILURE, 1);

		goto _php_parallel_exit;
	}

	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_READY, 1);

	do {
		php_parallel_schedule_el_t el;
        
		if (php_parallel_monitor_lock(runtime->monitor) != SUCCESS) {
			break;
		}

		if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_KILLED)) {
_php_parallel_kill:
			php_parallel_scheduler_kill(runtime);
			
			php_parallel_monitor_unlock(runtime->monitor);
			goto _php_parallel_exit;
		}

		while (!php_parallel_scheduler_pop(runtime, &el)) {
			if (!(state & PHP_PARALLEL_CLOSE)) {
				state = php_parallel_monitor_wait_locked(
						runtime->monitor,
						PHP_PARALLEL_EXEC|PHP_PARALLEL_CLOSE|PHP_PARALLEL_KILLED);
			}

			if ((state & (PHP_PARALLEL_CLOSE|PHP_PARALLEL_KILLED))) {
				if ((state & PHP_PARALLEL_KILLED)) {
					goto _php_parallel_kill;
				}

				if (php_parallel_scheduler_empty(runtime)) {
					php_parallel_monitor_unlock(runtime->monitor);
					goto _php_parallel_exit;
				}
			}
		}

		php_parallel_monitor_unlock(runtime->monitor);
        
		php_parallel_scheduler_run(runtime, el.frame);
	} while (1);

_php_parallel_exit:
	php_parallel_scheduler_exit(runtime);

	return NULL;
}

static void php_parallel_runtime_stop(php_parallel_runtime_t *runtime) {
    if (!runtime->monitor) {
        return;
    }
    
    php_parallel_monitor_lock(runtime->monitor);

	if (!php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
		php_parallel_monitor_set(
			runtime->monitor,
			PHP_PARALLEL_CLOSE, 0);

		if (!php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_FAILURE)) {
		    php_parallel_monitor_wait_locked(
			    runtime->monitor,
			    PHP_PARALLEL_DONE);
		}

		php_parallel_monitor_unlock(runtime->monitor);

		pthread_join(runtime->thread, NULL);
	} else {
		php_parallel_monitor_unlock(runtime->monitor);
	}

	php_parallel_monitor_destroy(runtime->monitor);

	if (runtime->bootstrap) {
		zend_string_release(runtime->bootstrap);
	}

    php_parallel_scheduler_destroy(runtime);
    
    runtime->monitor = NULL;
}

PHP_METHOD(Runtime, __construct)
{
	php_parallel_runtime_t *runtime = php_parallel_runtime_from(getThis());
	zend_string            *bootstrap = NULL;
	int32_t                 state = SUCCESS;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(bootstrap)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_invalid_arguments(
            "optional bootstrap expected");
        php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_FAILURE, 0);
        php_parallel_runtime_stop(runtime);
        return;
    );

	if (bootstrap) {
		runtime->bootstrap = zend_string_dup(bootstrap, 1);
	}

	if (pthread_create(&runtime->thread, NULL, php_parallel_runtime, runtime) != SUCCESS) {
		php_parallel_exception_ex(
		    php_parallel_runtime_error_ce, 
		    "cannot create thread %s", strerror(errno));
		php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_FAILURE, 0);
		php_parallel_runtime_stop(runtime);
		return;
	}

	state = php_parallel_monitor_wait(runtime->monitor, PHP_PARALLEL_READY|PHP_PARALLEL_FAILURE);

	if ((state == FAILURE) || (state & PHP_PARALLEL_FAILURE)) {
		php_parallel_exception_ex(
		    php_parallel_runtime_error_bootstrap_ce,
			"bootstrapping failed with %s", ZSTR_VAL(runtime->bootstrap));
		php_parallel_runtime_stop(runtime);
	}
}

PHP_METHOD(Runtime, run)
{
	php_parallel_runtime_t  *runtime = php_parallel_runtime_from(getThis());
	php_parallel_future_t   *future = NULL;
	zval *closure = NULL;
	zend_function *function;
	zval *argv = NULL;
	zend_bool returns = 0;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 2)
        Z_PARAM_OBJECT_OF_CLASS(closure, zend_ce_closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END_EX(
		php_parallel_invalid_arguments("Closure, or Closure and args expected");
		return;
    );
    
    if (!runtime->monitor) {
        php_parallel_exception_ex(
		    php_parallel_runtime_error_ce, 
		    "Runtime unusable");
		return;
    }
    
    if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
		php_parallel_exception_ex(
		    php_parallel_runtime_error_closed_ce, 
		    "Runtime closed");
		return;
	}

	php_parallel_monitor_lock(runtime->monitor);
	
	function = php_parallel_copy_check(
	            EG(current_execute_data)->prev_execute_data,
		        (zend_function*) zend_get_closure_method_def(closure), 
		        argv, &returns);

	if (!function) {
		php_parallel_monitor_unlock(runtime->monitor);
		return;
	}

	if (returns) {
		object_init_ex(return_value, php_parallel_future_ce);

		future = php_parallel_future_from(return_value);
		
		php_parallel_scheduler_push(
			runtime,
			future->monitor, 
			function, argv, &future->value);
	} else {
		php_parallel_scheduler_push(
		    runtime,
		    NULL,
		    function, argv, NULL);
	}
	
	php_parallel_monitor_set(runtime->monitor, PHP_PARALLEL_EXEC, 0);
	php_parallel_monitor_unlock(runtime->monitor);
}

PHP_METHOD(Runtime, close)
{
	php_parallel_runtime_t *runtime = 
		php_parallel_runtime_from(getThis());
		
	if (!runtime->monitor) {
	    php_parallel_exception_ex(
	        php_parallel_runtime_error_ce, 
	        "Runtime unusable");
	    return;
	}
	
	if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
	    php_parallel_exception_ex(
	        php_parallel_runtime_error_closed_ce, 
	        "Runtime closed");
	    return;
	}

	php_parallel_monitor_lock(runtime->monitor);

	php_parallel_monitor_set(
		runtime->monitor, PHP_PARALLEL_CLOSE, 0);
	php_parallel_monitor_wait_locked(
		runtime->monitor, PHP_PARALLEL_DONE);

	php_parallel_monitor_unlock(runtime->monitor);

	php_parallel_monitor_set(
		runtime->monitor, PHP_PARALLEL_CLOSED, 0);

	pthread_join(runtime->thread, NULL);
}

PHP_METHOD(Runtime, kill)
{
	php_parallel_runtime_t *runtime = 
		php_parallel_runtime_from(getThis());

	if (!runtime->monitor) {
	    php_parallel_exception_ex(
	        php_parallel_runtime_error_ce, 
	        "Runtime unusable");
	    return;
	}
	
	if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
	    php_parallel_exception_ex(
	        php_parallel_runtime_error_closed_ce, 
	        "Runtime closed");
	    return;
	}

	php_parallel_monitor_lock(runtime->monitor);

	php_parallel_monitor_set(
		runtime->monitor, PHP_PARALLEL_KILLED, 0);

	*(runtime->child.interrupt) = 1;

	php_parallel_monitor_wait_locked(
		runtime->monitor, PHP_PARALLEL_DONE);

	php_parallel_monitor_unlock(runtime->monitor);

	php_parallel_monitor_set(
		runtime->monitor, PHP_PARALLEL_CLOSED, 0);

	pthread_join(runtime->thread, NULL);
}

zend_function_entry php_parallel_runtime_methods[] = {
	PHP_ME(Runtime, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Runtime, run, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Runtime, close, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Runtime, kill, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

zend_object* php_parallel_runtime_create(zend_class_entry *type) {
	php_parallel_runtime_t *runtime = ecalloc(1, 
			sizeof(php_parallel_runtime_t) + zend_object_properties_size(type));

	zend_object_std_init(&runtime->std, type);

	runtime->std.handlers = &php_parallel_runtime_handlers;

	runtime->monitor = php_parallel_monitor_create();
    
    php_parallel_scheduler_init(runtime);

	runtime->parent.server = SG(server_context);

	return &runtime->std;
}

void php_parallel_runtime_destroy(zend_object *o) {
	php_parallel_runtime_t *runtime = 
		php_parallel_runtime_fetch(o);

	php_parallel_runtime_stop(runtime);
	
	zend_object_std_dtor(o);
}

void php_parallel_runtime_startup() {
    zend_class_entry ce;
    
    memcpy(&php_parallel_runtime_handlers, php_parallel_standard_handlers(), sizeof(zend_object_handlers));

	php_parallel_runtime_handlers.offset = XtOffsetOf(php_parallel_runtime_t, std);
	php_parallel_runtime_handlers.free_obj = php_parallel_runtime_destroy;

	INIT_NS_CLASS_ENTRY(ce, "parallel", "Runtime", php_parallel_runtime_methods);

	php_parallel_runtime_ce = zend_register_internal_class(&ce);
	php_parallel_runtime_ce->create_object = php_parallel_runtime_create;
	php_parallel_runtime_ce->ce_flags |= ZEND_ACC_FINAL;

	php_parallel_runtime_main = zend_new_interned_string(
	                        zend_string_init(ZEND_STRL(
	                            "\\parallel\\Runtime::run"), 1));
}

void php_parallel_runtime_shutdown() {
	zend_string_release(php_parallel_runtime_main);
}
#endif
