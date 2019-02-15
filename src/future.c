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
#ifndef HAVE_PARALLEL_FUTURE
#define HAVE_PARALLEL_FUTURE

#include "parallel.h"
#include "copy.h"
#include "future.h"

#include "zend_exceptions.h"

zend_class_entry *php_parallel_future_ce;
zend_object_handlers php_parallel_future_handlers;

PHP_METHOD(Future, value) 
{
	php_parallel_future_t *future = php_parallel_future_from(getThis());
	int32_t state;

	if (!Z_ISUNDEF(future->saved)) {
		ZVAL_COPY(return_value, &future->saved);
		return;
	}

	if ((state = php_parallel_monitor_wait(future->monitor, PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED)) == FAILURE) {
		php_parallel_exception(
			"an error occured while waiting for a value from Runtime");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE);
		return;
	}

	if (state & PHP_PARALLEL_ERROR) {
		php_parallel_exception(
			"an exception occured in Runtime, cannot retrieve value");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE);
		return;
	}

	if (state & PHP_PARALLEL_KILLED) {
		php_parallel_exception(
			"Runtime was killed, cannot retrieve value");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE);
		return;
	}

	if (Z_TYPE(future->value) != IS_NULL) {
		php_parallel_copy_zval(return_value, &future->value, 0);

		if (Z_REFCOUNTED(future->value)) {
			php_parallel_zval_dtor(&future->value);
		}

		ZVAL_COPY(&future->saved, return_value);
	} else {
		ZVAL_NULL(&future->saved);
	}

	php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE);
}

PHP_METHOD(Future, select)
{
	zval *resolving;
	zval *resolved;
	zval *errored;
	zend_long timeout = 0;
	zend_long idx;
	zval *future;
	
	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "aaa|l", &resolving, &resolved, &errored, &timeout) != SUCCESS) {
		php_parallel_exception(
			"resolving and resolved array with optional timeout expected");
		return;
	}

	if (timeout < 0) {
		php_parallel_exception(
			"optional timeout must be non-negative");
		return;
	}

	ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(resolving), idx, future) {
		int32_t state;
		php_parallel_future_t *f;

		if (Z_TYPE_P(future) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(future), php_parallel_future_ce)) {
			continue;
		}

		f = php_parallel_future_from(future);

		state = php_parallel_monitor_wait_timed(f->monitor, 
			PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED, timeout);		

		if (state == FAILURE) {
			
		}				
	} ZEND_HASH_FOREACH_END();
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_future_select_arginfo, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(1, resolving, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(1, resolved, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(1, errored, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

zend_function_entry php_parallel_future_methods[] = {
	PHP_ME(Future, value, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Future, select, php_parallel_future_select_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_FE_END
};

zend_object* php_parallel_future_create(zend_class_entry *type) {
	php_parallel_future_t *future = ecalloc(1, 
			sizeof(php_parallel_future_t) + zend_object_properties_size(type));

	zend_object_std_init(&future->std, type);

	future->std.handlers = &php_parallel_future_handlers;

	future->monitor = php_parallel_monitor_create();

	return &future->std;
}

void php_parallel_future_destroy(zend_object *o) {
	php_parallel_future_t *future = 
		php_parallel_future_fetch(o);

	if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_KILLED)) {
		php_parallel_monitor_wait(future->monitor, PHP_PARALLEL_READY);
		
		if (Z_REFCOUNTED(future->value)) {
			php_parallel_zval_dtor(&future->value);
		}
	}

	if (Z_REFCOUNTED(future->saved))
		zval_ptr_dtor(&future->saved);

	php_parallel_monitor_destroy(future->monitor);

	zend_object_std_dtor(o);
}

void php_parallel_future_startup() {
	zend_class_entry ce;

	memcpy(&php_parallel_future_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	php_parallel_future_handlers.offset = XtOffsetOf(php_parallel_future_t, std);
	php_parallel_future_handlers.free_obj = php_parallel_future_destroy;

	INIT_NS_CLASS_ENTRY(ce, "parallel", "Future", php_parallel_future_methods);

	php_parallel_future_ce = zend_register_internal_class(&ce);
	php_parallel_future_ce->create_object = php_parallel_future_create;
	php_parallel_future_ce->ce_flags |= ZEND_ACC_FINAL;
}
#endif
