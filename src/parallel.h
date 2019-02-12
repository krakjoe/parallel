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
#ifndef HAVE_PARALLEL_H
#define HAVE_PARALLEL_H

#include "php.h"
#include "monitor.h"
#include "pthread.h"
#include "parallel.h"

#ifndef _WIN32
#	include "sched.h"
#endif

extern zend_class_entry *php_parallel_ce;
extern zend_class_entry *php_parallel_future_ce;
extern zend_string *php_parallel_main;

typedef struct _php_parallel_entry_point_t {
	zend_function *point;
	zval argv;
	zval retval;
} php_parallel_entry_point_t;

typedef struct _php_parallel_stack_el_t {
	php_parallel_entry_point_t  entry;
	php_parallel_monitor_t      *monitor;
	zval                        *future;
} php_parallel_stack_el_t;

typedef struct _php_parallel_t {
	pthread_t                  thread;
	void                    ***creator;
	void                    ***context;
	php_parallel_monitor_t     *monitor;
	HashTable                  stack;
	HashPosition               next;
	zend_string                *bootstrap;
	zval                       configuration;
	zend_object                std;
} php_parallel_t;

void php_parallel_shutdown(void);
void php_parallel_startup(void);

static zend_always_inline php_parallel_t* php_parallel_fetch(zend_object *o) {
	return (php_parallel_t*) (((char*) o) - XtOffsetOf(php_parallel_t, std));
}

static zend_always_inline php_parallel_t* php_parallel_from(zval *z) {
	return php_parallel_fetch(Z_OBJ_P(z));
}

zend_object* php_parallel_create(zend_class_entry *);
void         php_parallel_destroy(zend_object *);

typedef struct _php_parallel_future_t {
	php_parallel_monitor_t *monitor;
	zval value;
	zval saved;
	zend_object std;
} php_parallel_future_t;

static zend_always_inline php_parallel_future_t* php_parallel_future_fetch(zend_object *o) {
	return (php_parallel_future_t*) (((char*) o) - XtOffsetOf(php_parallel_future_t, std));
}

static zend_always_inline php_parallel_future_t* php_parallel_future_from(zval *z) {
	return php_parallel_future_fetch(Z_OBJ_P(z));
}

zend_object* php_parallel_future_create(zend_class_entry *);
void         php_parallel_future_destroy(zend_object *);
#endif
