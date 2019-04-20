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
#ifndef HAVE_PARALLEL_TRACE_H
#define HAVE_PARALLEL_TRACE_H

#include "php.h"

typedef struct _php_parallel_trace_t php_parallel_trace_t;

struct _php_parallel_trace_t {
    zend_bool             thrown;
    zend_string           *class;
    zval                  file;
    zval                  line;
    zval                  message;
    zval                  stack;
};

zend_bool php_parallel_trace_caught(zend_execute_data *execute_data, zend_object *exception);
void php_parallel_trace_create(php_parallel_trace_t *trace, zval *throwable);
void php_parallel_trace_destroy(php_parallel_trace_t *trace);

ZEND_TLS zend_bool php_parallel_trace_catching;
#endif
