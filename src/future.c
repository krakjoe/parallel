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

#define php_parallel_timeout_exception(m, ...) zend_throw_exception_ex(php_parallel_timeout_exception_ce, 0, m, ##__VA_ARGS__)

zend_class_entry *php_parallel_future_ce;
zend_class_entry *php_parallel_timeout_exception_ce;
zend_object_handlers php_parallel_future_handlers;

PHP_METHOD(Future, value) 
{
	php_parallel_future_t *future = php_parallel_future_from(getThis());
	int32_t state;
	zend_long timeout = -1;

	if (ZEND_NUM_ARGS()) {
		if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "l", &timeout) != SUCCESS) {
			php_parallel_exception(
				"expected optional timeout");
			return;
		}

		if (timeout < 0) {
			php_parallel_exception(
				"expected timeout greater than or equal to 0");
			return;
		}
	}

	if (!Z_ISUNDEF(future->saved)) {
		ZVAL_COPY(return_value, &future->saved);
		return;
	}

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_DONE)) {
        /* resolved by select() not yet read */
        if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_KILLED|PHP_PARALLEL_ERROR)) {
            goto _php_parallel_future_done;
        }
        
        state = future->monitor->state;
    } else {
        if (timeout > -1) {
		    state = php_parallel_monitor_wait_timed(future->monitor, 
				    PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED, timeout);
	    } else {
		    state = php_parallel_monitor_wait(future->monitor, 
				    PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED);
	    }
    }

	if (state == ETIMEDOUT) {
		php_parallel_timeout_exception(
			"a timeout occured waiting for value from Runtime");
		return;
	}

	if (state == FAILURE) {
		php_parallel_exception(
			"an error occured while waiting for a value from Runtime");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_ERROR, 0);
		return;
	}

	if (state & PHP_PARALLEL_KILLED) {
		php_parallel_exception(
			"Runtime was killed, cannot retrieve value");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_KILLED, 0);
		return;
	}

	if (state & PHP_PARALLEL_ERROR) {
		php_parallel_exception(
			"an exception occured in Runtime, cannot retrieve value");
		php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_ERROR, 0);
		return;
	}

_php_parallel_future_done:
	if (Z_TYPE(future->value) != IS_NULL) {
		php_parallel_copy_zval(return_value, &future->value, 0);

		if (Z_REFCOUNTED(future->value)) {
			php_parallel_zval_dtor(&future->value);
		}

		ZVAL_COPY(&future->saved, return_value);
	} else {
		ZVAL_NULL(&future->saved);
	}

	php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE, 0);
}

PHP_METHOD(Future, select)
{
	zval *resolving = NULL;
	zval *resolved = NULL;
	zval *errored = NULL;
	zval *timedout = NULL;
	zend_long timeout = -1;
	zend_long idx;
	zval *future;

	if (ZEND_NUM_ARGS() == 3) {
		ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 3, 3)
#if PHP_VERSION_ID >= 70200
			Z_PARAM_ARRAY_EX2(resolving, 1, 1, 0)
			Z_PARAM_ARRAY_EX2(resolved,  1, 1, 1)
			Z_PARAM_ARRAY_EX2(errored,   1, 1, 1)
#else
			Z_PARAM_ARRAY_EX(resolving, 1, 0)
			Z_PARAM_ARRAY_EX(resolved,  1, 1)
			Z_PARAM_ARRAY_EX(errored,   1, 1)
#endif
		ZEND_PARSE_PARAMETERS_END_EX(
			php_parallel_exception(
				"expected (array $resolving, array $resolved, array $errored)");
			return;
		);
	} else if (ZEND_NUM_ARGS() == 5) {
		ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 5, 5)
#if PHP_VERSION_ID >= 70200
			Z_PARAM_ARRAY_EX2(resolving, 1, 1, 0)
			Z_PARAM_ARRAY_EX2(resolved,  1, 1, 1)
			Z_PARAM_ARRAY_EX2(errored,   1, 1, 1)
			Z_PARAM_ARRAY_EX2(timedout,  1, 1, 1)
#else
			Z_PARAM_ARRAY_EX(resolving, 1, 0)
			Z_PARAM_ARRAY_EX(resolved,  1, 1)
			Z_PARAM_ARRAY_EX(errored,   1, 1)
			Z_PARAM_ARRAY_EX(timedout,  1, 1)
#endif
			Z_PARAM_LONG(timeout)
		ZEND_PARSE_PARAMETERS_END_EX(
			if (timeout >= 0) {
				php_parallel_exception(
					"expected (array $resolving, array $resolved, array $errored, array $timedout, int $timeout)");
			} else {
				php_parallel_exception(
					"optional timeout must be non-negative");
			}
			return;
		);
	} else {
		php_parallel_exception(
			"expected (array $resolving, array $resolved, array $errored) or"
			        " (array $resolving, array $resolved, array $errored, array $timedout, int $timeout)");
		return;
	}

	if (!resolving || !resolved || !errored || (ZEND_NUM_ARGS() == 5 && !timedout)) {
		php_parallel_exception(
			"array arguments must not be null");
		return;
	} 

#if PHP_VERSION_ID < 70200
	ZVAL_DEREF(resolving);
	ZVAL_DEREF(resolved);
	ZVAL_DEREF(errored);

	if (ZEND_NUM_ARGS() == 5) {
		ZVAL_DEREF(timedout);
	}
#endif

	if (!zend_hash_num_elements(Z_ARRVAL_P(resolving))) {
		RETURN_LONG(0);
	}

	if (Z_TYPE_P(resolved) != IS_ARRAY) {
		array_init(resolved);
	} else zend_hash_clean(Z_ARRVAL_P(resolved));

	if (Z_TYPE_P(errored) != IS_ARRAY) {
		array_init(errored);
	} else zend_hash_clean(Z_ARRVAL_P(errored));

	if (ZEND_NUM_ARGS() == 5) {
		if (Z_TYPE_P(timedout) != IS_ARRAY) {
			array_init(timedout);
		} else zend_hash_clean(Z_ARRVAL_P(timedout));

		timeout = ceil(timeout / zend_hash_num_elements(Z_ARRVAL_P(resolving)));
	}

	ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(resolving), idx, future) {
		int32_t state;
		php_parallel_future_t *f;

		if (Z_TYPE_P(future) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(future), php_parallel_future_ce)) {
			zend_hash_index_del(Z_ARRVAL_P(resolving), idx);
			continue;
		}

		f = php_parallel_future_from(future);

		if (timeout > -1) {
			state = php_parallel_monitor_wait_timed(f->monitor, 
					PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED, 
					timeout);
		} else state = php_parallel_monitor_wait(f->monitor, 
					PHP_PARALLEL_READY|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED);

		if (state == FAILURE) {
			/* failed to wait, internal error */
			zend_hash_index_update(Z_ARRVAL_P(errored), idx, future);
			Z_ADDREF_P(future);
			php_parallel_monitor_set(f->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_ERROR, 0);
			zend_hash_index_del(Z_ARRVAL_P(resolving), idx);		
		} else if (state == ETIMEDOUT) {
			/* timeout occured while waiting */
			zend_hash_index_update(Z_ARRVAL_P(timedout), idx, future);
			Z_ADDREF_P(future);
		} else if (state & (PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED)) {
			/* exception or killed */
			zend_hash_index_update(Z_ARRVAL_P(errored), idx, future);
			Z_ADDREF_P(future);
			if (state & PHP_PARALLEL_ERROR) {
				php_parallel_monitor_set(f->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_ERROR, 0);
			} else 	php_parallel_monitor_set(f->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_KILLED, 0);

			zend_hash_index_del(Z_ARRVAL_P(resolving), idx);
		} else {
			/* resolved */
			zend_hash_index_update(Z_ARRVAL_P(resolved), idx, future);
			Z_ADDREF_P(future);
			php_parallel_monitor_set(f->monitor, PHP_PARALLEL_DONE, 0);
			zend_hash_index_del(Z_ARRVAL_P(resolving), idx);
		}

		if (zend_hash_num_elements(Z_ARRVAL_P(resolved))) {
			/* break out on first resolution */
			break;
		}
	} ZEND_HASH_FOREACH_END();

	if (ZEND_NUM_ARGS() == 5) {
		RETURN_LONG(
			zend_hash_num_elements(Z_ARRVAL_P(resolved)) + 
			zend_hash_num_elements(Z_ARRVAL_P(errored)) +
			zend_hash_num_elements(Z_ARRVAL_P(timedout)));
	} else {
		RETURN_LONG(
			zend_hash_num_elements(Z_ARRVAL_P(resolved)) +
			zend_hash_num_elements(Z_ARRVAL_P(errored)));
	}
}

PHP_METHOD(Future, done)
{
	php_parallel_future_t *future = 
		php_parallel_future_from(getThis());

	RETURN_BOOL(php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY|PHP_PARALLEL_DONE));
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_future_select_arginfo, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(1, resolving, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(1, resolved, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(1, errored, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(1, timedout, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

zend_function_entry php_parallel_future_methods[] = {
	PHP_ME(Future, value, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Future, done, NULL, ZEND_ACC_PUBLIC)
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

	if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_DONE|PHP_PARALLEL_ERROR|PHP_PARALLEL_KILLED)) {
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

	INIT_NS_CLASS_ENTRY(ce, "parallel", "TimeoutException", NULL);

	php_parallel_timeout_exception_ce = zend_register_internal_class_ex(&ce, php_parallel_exception_ce);
}
#endif
