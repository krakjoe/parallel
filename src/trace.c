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
#ifndef HAVE_PARALLEL_TRACE
#define HAVE_PARALLEL_TRACE

#include "php.h"
#include "parallel.h"
#include "copy.h"
#include "trace.h"

zend_bool php_parallel_trace_caught(zend_execute_data *execute_data, zend_object *exception) {
	const zend_op *op;
	zend_op *cur;
	uint32_t op_num, i;
	zend_op_array *op_array = &execute_data->func->op_array;

	if (execute_data->opline >= EG(exception_op) && execute_data->opline < EG(exception_op) + 3) {
		op = EG(opline_before_exception);
	} else {
		op = execute_data->opline;
	}

	op_num = op - op_array->opcodes;

	for (i = 0; i < op_array->last_try_catch && op_array->try_catch_array[i].try_op <= op_num; i++) {
		uint32_t catch = op_array->try_catch_array[i].catch_op, finally = op_array->try_catch_array[i].finally_op;
		if (op_num <= catch || op_num <= finally) {
			if (finally) {
				return 1;
			}

			cur = &op_array->opcodes[catch];
			while (1) {
				zend_class_entry *ce;

				if (!(ce = CACHED_PTR(cur->extended_value & ~ZEND_LAST_CATCH))) {
					ce = zend_fetch_class_by_name(Z_STR_P(RT_CONSTANT(cur, cur->op1)), Z_STR_P(RT_CONSTANT(cur, cur->op1) + 1), ZEND_FETCH_CLASS_NO_AUTOLOAD);
					CACHE_PTR(cur->extended_value & ~ZEND_LAST_CATCH, ce);
				}

				if (ce == exception->ce || (ce && instanceof_function(exception->ce, ce))) {
					return 1;
				}

				if (cur->extended_value & ZEND_LAST_CATCH) {
					return 0;
				}

				cur = OP_JMP_ADDR(cur, cur->op2);
			}

			return 0;
		}
	}

	return op->opcode == ZEND_CATCH;
}

void php_parallel_trace_throwable_property(zval *throwable, int known, zval *return_value) {
    zval rv, 
#if PHP_VERSION_ID < 80000
         key, 
#endif
         *property;

    EG(fake_scope) = Z_OBJCE_P(throwable);
    
#if PHP_VERSION_ID < 80000
    ZVAL_STR(&key, ZSTR_KNOWN(known));
    
    property = zend_std_read_property(throwable, &key, BP_VAR_R, NULL, &rv);
#else
    property = zend_std_read_property(Z_OBJ_P(throwable), ZSTR_KNOWN(known), BP_VAR_R, NULL, &rv);
#endif

    php_parallel_copy_zval(return_value, property, 1);
}

void php_parallel_trace_create(php_parallel_trace_t *trace, zval *throwable) {
    if (trace->thrown) {
        php_parallel_trace_destroy(trace);
    }
    
    php_parallel_trace_catching = 1;
    
    trace->class = zend_string_dup(Z_OBJCE_P(throwable)->name, 1);
    
    php_parallel_trace_throwable_property(throwable, ZEND_STR_MESSAGE, &trace->message);
    php_parallel_trace_throwable_property(throwable, ZEND_STR_FILE, &trace->file);
    php_parallel_trace_throwable_property(throwable, ZEND_STR_LINE, &trace->line);
    php_parallel_trace_throwable_property(throwable, ZEND_STR_TRACE, &trace->stack);
    
    php_printf("%s %s in %s on line %d\n", ZSTR_VAL(trace->class), Z_STRVAL(trace->message), Z_STRVAL(trace->file), Z_LVAL(trace->line));
    php_var_dump(&trace->stack, 0);
    
    trace->thrown = 1;
    
    php_parallel_trace_catching = 0;
}

void php_parallel_trace_destroy(php_parallel_trace_t *trace) {
    if (!trace->thrown) {
        return;
    }
    
    zend_string_release(trace->class);
    
    php_parallel_zval_dtor(&trace->message);
    php_parallel_zval_dtor(&trace->file);
    php_parallel_zval_dtor(&trace->stack);
}
#endif
