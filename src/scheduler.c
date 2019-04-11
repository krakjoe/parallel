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

#include "zend_exceptions.h"
#include "zend_vm.h"

TSRM_TLS php_parallel_t*         php_parallel_scheduler_context = NULL;
TSRM_TLS zend_llist*             php_parallel_scheduler_list = NULL;

TSRM_TLS php_parallel_monitor_t* php_parallel_scheduler_monitor = NULL;
TSRM_TLS zval*                   php_parallel_scheduler_future = NULL;

#define PHP_PARALLEL_CALL_YIELD (1<<12)
#define PHP_PARALLEL_CALL_YIELDED(f)  \
    ((ZEND_CALL_INFO((f)) & PHP_PARALLEL_CALL_YIELD) == PHP_PARALLEL_CALL_YIELD)

static void php_parallel_schedule_free(void *scheduleed) {
  	php_parallel_schedule_el_t *el = 
	    (php_parallel_schedule_el_t*) scheduleed;
	zend_bool yielded = PHP_PARALLEL_CALL_YIELDED(el->frame);
	
    if (!yielded) {
        zval *slot = (zval*) ZEND_CALL_ARG(el->frame, 1),
             *end = slot + ZEND_CALL_NUM_ARGS(el->frame);
        
        while (slot < end) {
            php_parallel_zval_dtor(slot);
            
            slot++;
        }
    
        php_parallel_copy_free(el->frame->func, 1);
    }
    
    pefree(el->frame, !yielded);
}

void php_parallel_scheduler_init(zend_llist *schedule) {
	zend_llist_init(schedule, sizeof(php_parallel_schedule_el_t), php_parallel_schedule_free, 1);
}

void php_parallel_scheduler_destroy(zend_llist *schedule) {
    zend_llist_destroy(schedule);
}

php_parallel_t* php_parallel_scheduler_setup(php_parallel_t *parallel) {
    php_parallel_scheduler_context = parallel;
   
    ts_resource(0);

    TSRMLS_CACHE_UPDATE();

    SG(server_context) = parallel->parent.server;

    parallel->child.interrupt = &EG(vm_interrupt);
    
    php_parallel_scheduler_list = &parallel->schedule;
    
    return parallel;
}

void php_parallel_scheduler_push(
            zend_llist *schedule, 
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
	
	el.frame   = frame;
	el.monitor = monitor;

	zend_llist_add_element(schedule, &el);
}

static zend_always_inline int php_parallel_scheduler_delete(void *lhs, void *rhs) {
    return lhs == rhs;
}

zend_bool php_parallel_scheduler_pop(php_parallel_schedule_el_t *el) {
	php_parallel_schedule_el_t *head;
	zend_bool yielded;
	
	if (zend_llist_count(php_parallel_scheduler_list) == 0) {
	    return 0;
	}
	
	head = zend_llist_get_first(php_parallel_scheduler_list);
	
	yielded = PHP_PARALLEL_CALL_YIELDED(head->frame);
	
	*el = *head;
	
	if (!yielded) {
        el->frame = zend_vm_stack_push_call_frame(
	        ZEND_CALL_TOP_FUNCTION, 
	        php_parallel_copy(head->frame->func, 0), 
	        ZEND_CALL_NUM_ARGS(head->frame), NULL, NULL);
	    
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
            el->frame->return_value);
    } else {
        el->frame = zend_vm_stack_push_call_frame(  
            ZEND_CALL_INFO(head->frame),
            head->frame->func,
            ZEND_CALL_NUM_ARGS(head->frame), NULL, NULL);
        
        zend_init_func_execute_data(
            el->frame, 
            &el->frame->func->op_array, 
            el->frame->return_value);
        
        memcpy((zval*) el->frame  + ZEND_CALL_FRAME_SLOT, 
              ((zval*) head->frame) + ZEND_CALL_FRAME_SLOT,
              ZEND_MM_ALIGNED_SIZE(sizeof(zval) *
                (head->frame->func->op_array.last_var +
                 head->frame->func->op_array.T)));
                
        el->frame->opline = head->frame->opline + 1;
    }
    
    el->frame->return_value = head->frame->return_value;
      
	zend_llist_del_element(
	    php_parallel_scheduler_list, 
	    head, 
	    php_parallel_scheduler_delete);

	return 1;
}

void php_parallel_scheduler_yield(zend_execute_data *frame) {
    php_parallel_schedule_el_t el;
    size_t used = zend_vm_calc_used_stack(
        ZEND_CALL_NUM_ARGS(frame), frame->func);
        
    php_parallel_scheduler_context->monitor->state |= PHP_PARALLEL_YIELDING;
        
    php_parallel_monitor_lock(php_parallel_scheduler_context->monitor);
    
    el.frame = (zend_execute_data*) ecalloc(1, used);
	
	memcpy(el.frame, frame, used);
	
	el.frame->return_value = php_parallel_scheduler_future;
	el.monitor             = php_parallel_scheduler_monitor;

	ZEND_ADD_CALL_FLAG(el.frame, PHP_PARALLEL_CALL_YIELD);

	zend_llist_add_element(php_parallel_scheduler_list, &el);
	
    php_parallel_monitor_set(
        php_parallel_scheduler_context->monitor, PHP_PARALLEL_EXEC, 0);            
    php_parallel_monitor_unlock(php_parallel_scheduler_context->monitor);
}

void php_parallel_scheduler_run(php_parallel_monitor_t *monitor, zend_execute_data *frame) {
	zval *future = NULL;
    zval retval;
    zend_bool yielding = 0;
    
    php_parallel_scheduler_monitor = monitor;

	if (frame->return_value) {
        php_parallel_scheduler_future = future = frame->return_value;
	} else {
	    php_parallel_scheduler_future = future = NULL;
	}
	
	frame->return_value = &retval;
	
	ZVAL_UNDEF(&retval);
	
	zend_try {
	    zend_execute_ex(frame);
		
		if (EG(exception)) {
		    if (!EG(current_execute_data)) {
		        zend_throw_exception_internal(NULL);
		    } else if (EG(current_execute_data)->func &&
		               ZEND_USER_CODE(EG(current_execute_data)->func->common.type)) {
		        zend_rethrow_exception(EG(current_execute_data));
		    }
		}
	} zend_catch {
	    yielding = php_parallel_monitor_wipe(php_parallel_scheduler_context->monitor, PHP_PARALLEL_YIELDING);
	    
        if (!yielding) {
            if (monitor) {
			    if (php_parallel_monitor_check(php_parallel_scheduler_context->monitor, PHP_PARALLEL_KILLED)) {
				    php_parallel_monitor_set(monitor, 
					    PHP_PARALLEL_KILLED|PHP_PARALLEL_ERROR, 0);
			    } else {
				    php_parallel_monitor_set(monitor, PHP_PARALLEL_ERROR, 0);
			    }
		    }
        }
	} zend_end_try();

    if (!yielding) {
        if (!Z_ISUNDEF(retval)) {
            if (future) {
                php_parallel_copy_zval(
                    future, frame->return_value, 1);
            }

		    if (Z_REFCOUNTED(retval)) {
			    zval_ptr_dtor(&retval);
		    }
	    }
	
        php_parallel_copy_free(frame->func, 0);
    }
    
    zend_vm_stack_free_call_frame(frame);
}

zend_bool php_parallel_scheduler_may_yield(zend_execute_data *current, zend_execute_data *frame) {
    return php_parallel_scheduler_context && current->prev_execute_data == frame;
}

static void _php_parallel_scheduler_kill(void *scheduled) {
	php_parallel_schedule_el_t *el = 
	    (php_parallel_schedule_el_t*) scheduled;

	if (el->monitor) {
		php_parallel_monitor_set(el->monitor, PHP_PARALLEL_KILLED, 1);
	}
}

zend_bool php_parallel_scheduler_killed() {
    if (php_parallel_scheduler_context && 
	    php_parallel_monitor_check(
	        php_parallel_scheduler_context->monitor, 
	        PHP_PARALLEL_KILLED)) {
	    return 1;
	}
	return 0;
}

void php_parallel_scheduler_kill() {
    zend_llist_apply(php_parallel_scheduler_list, _php_parallel_scheduler_kill);
    zend_llist_clean(php_parallel_scheduler_list);
}
#endif
