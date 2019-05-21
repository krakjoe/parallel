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
#ifndef HAVE_PARALLEL_FUTURE_H
#define HAVE_PARALLEL_FUTURE_H

#include "php.h"
#include "monitor.h"

extern zend_class_entry *php_parallel_future_ce;

typedef struct _php_parallel_future_t {
    php_parallel_monitor_t *monitor;
    php_parallel_runtime_t *runtime;
    void *handle;
    zval value;
    zend_object std;
} php_parallel_future_t;

static zend_always_inline php_parallel_future_t* php_parallel_future_fetch(zend_object *o) {
    return (php_parallel_future_t*) (((char*) o) - XtOffsetOf(php_parallel_future_t, std));
}

static zend_always_inline php_parallel_future_t* php_parallel_future_from(zval *z) {
    return php_parallel_future_fetch(Z_OBJ_P(z));
}

static zend_always_inline php_parallel_future_t* php_parallel_future_construct(zval *retval) {
        object_init_ex(retval, php_parallel_future_ce);

        return php_parallel_future_from(retval);
}

zend_object* php_parallel_future_create(zend_class_entry *);
void         php_parallel_future_destroy(zend_object *);

zend_bool    php_parallel_future_lock(php_parallel_future_t *future);
zend_bool    php_parallel_future_readable(php_parallel_future_t *future);
void         php_parallel_future_value(php_parallel_future_t *future, zval *value);
zend_bool    php_parallel_future_unlock(php_parallel_future_t *future);

PHP_MINIT_FUNCTION(PARALLEL_FUTURE);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_FUTURE);
#endif
