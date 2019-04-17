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
#include "copy.h"
#include "scheduler.h"

#include "SAPI.h"

#include "php_main.h"

#include "zend_exceptions.h"
#include "zend_vm.h"

TSRM_TLS php_parallel_t* php_parallel_scheduler_context = NULL;

void (*zend_interrupt_handler)(zend_execute_data*) = NULL;

static zend_always_inline int php_parallel_scheduler_list_delete(void *lhs, void *rhs) {
    return lhs == rhs;
}

void php_parallel_scheduler_interrupt(zend_execute_data *execute_data) {
	if (php_parallel_scheduler_context && 
	    php_parallel_monitor_check(
	        php_parallel_scheduler_context->monitor, 
	        PHP_PARALLEL_KILLED)) {
		zend_bailout();
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
        php_parallel_zval_dtor(slot);

        slot++;
    }

    php_parallel_copy_free(el->frame->func, 1);
    pefree(el->frame, 1);
}

void php_parallel_scheduler_init(php_parallel_t *parallel) {
    zend_llist_init(&parallel->schedule, sizeof(php_parallel_schedule_el_t), php_parallel_schedule_free, 1);
}

void php_parallel_scheduler_destroy(php_parallel_t *parallel) {
    zend_llist_destroy(&parallel->schedule);
}

php_parallel_t* php_parallel_scheduler_setup(php_parallel_t *parallel) {
    php_parallel_scheduler_context = parallel;

    ts_resource(0);

    TSRMLS_CACHE_UPDATE();

    SG(server_context) = parallel->parent.server;

    parallel->child.interrupt = &EG(vm_interrupt);

    PG(expose_php)       = 0;
    PG(auto_globals_jit) = 1;

    php_request_startup();

    zend_disable_function(ZEND_STRL("setlocale"));
#if PHP_VERSION_ID < 70400
    zend_disable_function(ZEND_STRL("putenv"));
#endif

    PG(during_request_startup)  = 0;
    SG(sapi_started)            = 0;
    SG(headers_sent)            = 1;
    SG(request_info).no_headers = 1;

    return parallel;
}

void php_parallel_scheduler_exit(php_parallel_t *parallel) {
    php_parallel_monitor_set(
        parallel->monitor, 
        PHP_PARALLEL_DONE, 1);

    php_request_shutdown(NULL);

    ts_free_thread();

    pthread_exit(NULL);
}

void php_parallel_scheduler_push(
            php_parallel_t *parallel, 
            php_parallel_monitor_t *monitor, 
            zend_function *function, 
            zval *argv, 
            zval *future) {
    php_parallel_schedule_el_t el;
    uint32_t argc = argv && Z_TYPE_P(argv) == IS_ARRAY ?
                        zend_hash_num_elements(Z_ARRVAL_P(argv)) : 0;
                        
	zend_execute_data *frame = 
	    (zend_execute_data*) 
	        pecalloc(1, zend_vm_calc_used_stack(argc, function), 1);

    frame->func = php_parallel_copy(function, 1);
    
	if (argv && Z_TYPE_P(argv) == IS_ARRAY) {
	    zval *slot = ZEND_CALL_ARG(frame, 1);
	    zval *param;
	    uint32_t argc = 0;
	    
	    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(argv), param) {
	        php_parallel_copy_zval(slot, param, 1);
	        slot++;
	        argc++;
	    } ZEND_HASH_FOREACH_END();
	    
	    ZEND_CALL_NUM_ARGS(frame) = argc;
	} else {
	    ZEND_CALL_NUM_ARGS(frame) = 0;
	}
	
	frame->return_value = future;
	
	if (monitor) {
	    Z_PTR(frame->This) = monitor;
	}
	
	el.frame   = frame;

	zend_llist_add_element(&parallel->schedule, &el);
}

zend_bool php_parallel_scheduler_empty(php_parallel_t *parallel) {
    return !zend_llist_count(&parallel->schedule);
}

zend_bool php_parallel_scheduler_pop(php_parallel_t *parallel, php_parallel_schedule_el_t *el) {
	php_parallel_schedule_el_t *head;
	
	if (zend_llist_count(&parallel->schedule) == 0) {
	    return 0;
	}
	
	head = zend_llist_get_first(&parallel->schedule);
	
	*el = *head;
	
	el->frame = zend_vm_stack_push_call_frame(
        ZEND_CALL_TOP_FUNCTION, 
        php_parallel_copy(head->frame->func, 0), 
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
            php_parallel_copy_zval(param, slot, 0);
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
	    &parallel->schedule, 
	    head, 
	    php_parallel_scheduler_list_delete);

	return 1;
}

void php_parallel_scheduler_run(php_parallel_t *parallel, zend_execute_data *frame) {
    php_parallel_monitor_t *monitor = Z_PTR(frame->This);
    
    zend_first_try {
	    zend_try {
	        zend_execute_ex(frame);
		    
		    if (UNEXPECTED(EG(exception))) {
		        if (UNEXPECTED(!EG(current_execute_data))) {
		            zend_throw_exception_internal(NULL);
		        } else if (EG(current_execute_data)->func &&
		                   ZEND_USER_CODE(EG(current_execute_data)->func->common.type)) {
		            zend_rethrow_exception(EG(current_execute_data));
		        }
		    }
	    } zend_catch {
            if (monitor) {
		        if (php_parallel_monitor_check(parallel->monitor, PHP_PARALLEL_KILLED)) {
			        php_parallel_monitor_set(monitor, 
				        PHP_PARALLEL_KILLED|PHP_PARALLEL_ERROR, 0);
		        } else {
			        php_parallel_monitor_set(monitor, PHP_PARALLEL_ERROR, 0);
		        }
	        }
	    } zend_end_try();

         if (frame->return_value  && !Z_ISUNDEF_P(frame->return_value)) {
            zval garbage = *frame->return_value;
            
	        if (Z_REFCOUNTED(garbage)) {
	            php_parallel_copy_zval(
                    frame->return_value, 
                    frame->return_value, 1);
                    
		        zval_ptr_dtor(&garbage);
	        }
        }
    
        php_parallel_copy_free(frame->func, 0);
    
        zend_vm_stack_free_call_frame(frame);
    } zend_end_try ();

    if (monitor) {
		php_parallel_monitor_set(monitor, PHP_PARALLEL_READY, 1);
	}
}

static void _php_parallel_scheduler_kill(void *scheduled) {
	php_parallel_schedule_el_t *el = 
	    (php_parallel_schedule_el_t*) scheduled;

	if (Z_PTR(el->frame->This)) {
		php_parallel_monitor_set(Z_PTR(el->frame->This), PHP_PARALLEL_KILLED, 1);
	}
}

void php_parallel_scheduler_kill(php_parallel_t *parallel) {
    zend_llist_apply(&parallel->schedule, _php_parallel_scheduler_kill);
    zend_llist_clean(&parallel->schedule);
}
#endif
