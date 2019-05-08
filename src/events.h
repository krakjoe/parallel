/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
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
#ifndef HAVE_PARALLEL_EVENTS_H
#define HAVE_PARALLEL_EVENTS_H

typedef struct _php_parallel_events_t {
    zval        input;
    HashTable   targets;
    zend_long   timeout;
    zend_bool   blocking;
    zend_object std;
} php_parallel_events_t;

typedef enum {
    PHP_PARALLEL_EVENTS_LINK = 1,
    PHP_PARALLEL_EVENTS_FUTURE
} php_parallel_events_type_t;

typedef struct _php_parallel_events_state_t {
    php_parallel_events_type_t type;
    zend_string *name;
    zend_bool readable;
    zend_bool writable;
    zend_bool closed;
    zend_object *object;
} php_parallel_events_state_t;

#define php_parallel_events_state_initializer {0, NULL, 0, 0, NULL}

static zend_always_inline php_parallel_events_t* php_parallel_events_fetch(zend_object *o) {
    return (php_parallel_events_t*) (((char*) o) - XtOffsetOf(php_parallel_events_t, std));
}

static zend_always_inline php_parallel_events_t* php_parallel_events_from(zval *z) {
    return php_parallel_events_fetch(Z_OBJ_P(z));
}

extern zend_class_entry* php_parallel_events_ce;

PHP_MINIT_FUNCTION(PARALLEL_EVENTS);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS);
#endif
