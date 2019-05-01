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
#ifndef HAVE_PARALLEL_SCHEDULER
#define HAVE_PARALLEL_SCHEDULER

#include "parallel.h"

TSRM_TLS php_parallel_runtime_t* php_parallel_scheduler_context = NULL;
TSRM_TLS php_parallel_future_t* php_parallel_scheduler_future = NULL;

void (*zend_interrupt_handler)(zend_execute_data*) = NULL;

static zend_always_inline int php_parallel_scheduler_list_delete(void *lhs, void *rhs) {
    return lhs == rhs;
}

void php_parallel_scheduler_interrupt(zend_execute_data *execute_data) {
    if (php_parallel_scheduler_context) {
        if (php_parallel_monitor_check(
            php_parallel_scheduler_context->monitor,
            PHP_PARALLEL_KILLED)) {
            zend_bailout();
        }

        if (php_parallel_monitor_check(
            php_parallel_scheduler_future->monitor,
            PHP_PARALLEL_CANCELLED)) {
            zend_bailout();
        }
    }

    if (zend_interrupt_handler) {
        zend_interrupt_handler(execute_data);
    }
}

void php_parallel_scheduler_startup() {
    zend_interrupt_handler = zend_interrupt_function;
    zend_interrupt_function = php_parallel_scheduler_interrupt;
}

void php_parallel_scheduler_shutdown() {
    zend_interrupt_function = zend_interrupt_handler;
}

static void php_parallel_schedule_free(void *scheduleed) {
    php_parallel_schedule_el_t *el =
        (php_parallel_schedule_el_t*) scheduleed;
    zval *slot = (zval*) ZEND_CALL_ARG(el->frame, 1),
         *end = slot + ZEND_CALL_NUM_ARGS(el->frame);

    while (slot < end) {
        PARALLEL_ZVAL_DTOR(slot);
        slot++;
    }

    pefree(el->frame, 1);
}

void php_parallel_scheduler_init(php_parallel_runtime_t *runtime) {
    zend_llist_init(&runtime->schedule, sizeof(php_parallel_schedule_el_t), php_parallel_schedule_free, 1);
}

void php_parallel_scheduler_destroy(php_parallel_runtime_t *runtime) {
    zend_llist_destroy(&runtime->schedule);
}

php_parallel_runtime_t* php_parallel_scheduler_setup(php_parallel_runtime_t *runtime) {
    php_parallel_scheduler_context = runtime;

    ts_resource(0);

    TSRMLS_CACHE_UPDATE();

    SG(server_context) = runtime->parent.server;

    runtime->child.interrupt = &EG(vm_interrupt);

    PG(expose_php)       = 0;
    PG(auto_globals_jit) = 1;

    php_request_startup();

    zend_disable_function(ZEND_STRL("setlocale"));
#if PHP_VERSION_ID < 70400
    zend_disable_function(ZEND_STRL("putenv"));
#endif
    zend_disable_function(ZEND_STRL("dl"));

    PG(during_request_startup)  = 0;
    SG(sapi_started)            = 0;
    SG(headers_sent)            = 1;
    SG(request_info).no_headers = 1;

    return runtime;
}

void php_parallel_scheduler_exit(php_parallel_runtime_t *runtime) {
    php_parallel_monitor_set(
        runtime->monitor,
        PHP_PARALLEL_DONE, 1);

    php_request_shutdown(NULL);

    ts_free_thread();

    pthread_exit(NULL);
}

void php_parallel_scheduler_push(
            php_parallel_runtime_t *runtime,
            php_parallel_future_t *future,
            zend_function *function,
            zval *argv) {
    php_parallel_schedule_el_t el;
    uint32_t argc = argv && Z_TYPE_P(argv) == IS_ARRAY ?
                        zend_hash_num_elements(Z_ARRVAL_P(argv)) : 0;

    zend_execute_data *frame =
        (zend_execute_data*)
            pecalloc(1, zend_vm_calc_used_stack(argc, function), 1);

    frame->func = php_parallel_copy_function(function, 1);

    if (argv && Z_TYPE_P(argv) == IS_ARRAY) {
        zval *slot = ZEND_CALL_ARG(frame, 1);
        zval *param;
        uint32_t argc = 0;

        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(argv), param) {
            PARALLEL_ZVAL_COPY(slot, param, 1);
            slot++;
            argc++;
        } ZEND_HASH_FOREACH_END();

        ZEND_CALL_NUM_ARGS(frame) = argc;
    } else {
        ZEND_CALL_NUM_ARGS(frame) = 0;
    }

    if (future) {
        frame->return_value = 
                &future->value;
        Z_PTR(frame->This) = 
                future;
    }

    el.frame   = frame;

    zend_llist_add_element(&runtime->schedule, &el);

    if (future) {
        future->runtime = runtime;
        future->handle = 
            runtime->schedule.tail->data;
        GC_ADDREF(&runtime->std);
    }
}

zend_bool php_parallel_scheduler_empty(php_parallel_runtime_t *runtime) {
    return !zend_llist_count(&runtime->schedule);
}

zend_bool php_parallel_scheduler_pop(php_parallel_runtime_t *runtime, php_parallel_schedule_el_t *el) {
    php_parallel_schedule_el_t *head;

    if (zend_llist_count(&runtime->schedule) == 0) {
        return 0;
    }

    head = zend_llist_get_first(&runtime->schedule);

    *el = *head;

    el->frame = zend_vm_stack_push_call_frame(
        ZEND_CALL_TOP_FUNCTION,
        php_parallel_copy_function(head->frame->func, 0),
        ZEND_CALL_NUM_ARGS(head->frame),
#if PHP_VERSION_ID < 70400
        NULL,
#endif
        NULL);

    if (ZEND_CALL_NUM_ARGS(head->frame)) {
        zval *slot = (zval*) ZEND_CALL_ARG(head->frame, 1),
             *end  = slot + ZEND_CALL_NUM_ARGS(head->frame);
        zval *param = ZEND_CALL_ARG(el->frame, 1);

        while (slot < end) {
            PARALLEL_ZVAL_COPY(param, slot, 0);
            slot++;
            param++;
        }
    }

    zend_init_func_execute_data(
        el->frame,
        &el->frame->func->op_array,
        head->frame->return_value);

    Z_PTR(el->frame->This) = Z_PTR(head->frame->This);

    zend_llist_del_element(
        &runtime->schedule,
        head,
        php_parallel_scheduler_list_delete);

    return 1;
}

void php_parallel_scheduler_run(php_parallel_runtime_t *runtime, zend_execute_data *frame) {
    php_parallel_scheduler_future = (php_parallel_future_t*) Z_PTR(frame->This);

    zend_first_try {
        
        zend_try {
            zend_execute_ex(frame);

            if (UNEXPECTED(EG(exception))) {
                if (php_parallel_scheduler_future) {
                    php_parallel_exceptions_save(
                        frame->return_value, EG(exception));

                    zend_clear_exception();

                    php_parallel_monitor_set(
                        php_parallel_scheduler_future->monitor, 
                        PHP_PARALLEL_ERROR, 1);
                } else {
                    zend_throw_exception_internal(NULL);
                }
            }
        } zend_catch {
            if (php_parallel_scheduler_future) {
                if (!php_parallel_monitor_check(
                        php_parallel_scheduler_future->monitor, 
                        PHP_PARALLEL_CANCELLED)) {
                    php_parallel_monitor_set(
                        php_parallel_scheduler_future->monitor, 
                        PHP_PARALLEL_KILLED, 0);
                }
            }
        } zend_end_try();

         if (frame->return_value  && !Z_ISUNDEF_P(frame->return_value)) {
            zval garbage = *frame->return_value;

            if (Z_OPT_REFCOUNTED(garbage)) {
                PARALLEL_ZVAL_COPY(
                    frame->return_value, &garbage, 1);

                zval_ptr_dtor(&garbage);
            }
        }

        php_parallel_copy_function_free(frame->func, 0);

        zend_vm_stack_free_call_frame(frame);
    } zend_end_try ();

    if (php_parallel_scheduler_future) {
        php_parallel_monitor_set(
            php_parallel_scheduler_future->monitor, 
            PHP_PARALLEL_READY, 1);
    }
}

void php_parallel_scheduler_kill(php_parallel_runtime_t *runtime) {
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

zend_bool php_parallel_scheduler_cancel(php_parallel_future_t *future) {
    size_t in, out = 0;

    php_parallel_monitor_lock(future->runtime->monitor);

    in = zend_llist_count(&future->runtime->schedule);

    zend_llist_del_element(
        &future->runtime->schedule,
        future->handle,
        php_parallel_scheduler_list_delete);

    out = zend_llist_count(&future->runtime->schedule);

    if (in == out) {
        zend_bool cancelled = 0;

        php_parallel_monitor_lock(future->monitor);

        if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY)) {
            *(future->runtime->child.interrupt) = 1;

            php_parallel_monitor_set(future->monitor, PHP_PARALLEL_CANCELLED, 0);
            php_parallel_monitor_wait_locked(future->monitor,
                PHP_PARALLEL_READY);
            php_parallel_monitor_set(future->monitor,
                PHP_PARALLEL_READY|PHP_PARALLEL_DONE, 0);

            cancelled = 1;
        }

        php_parallel_monitor_unlock(future->monitor);

        php_parallel_monitor_unlock(future->runtime->monitor);
        return cancelled;
    } else {
        php_parallel_monitor_set(future->monitor,
            PHP_PARALLEL_READY|
            PHP_PARALLEL_DONE|
            PHP_PARALLEL_CANCELLED, 1);

        php_parallel_monitor_unlock(future->runtime->monitor);
        return 1;
    }
}
#endif
