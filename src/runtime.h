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
#ifndef HAVE_PARALLEL_RUNTIME_H
#define HAVE_PARALLEL_RUNTIME_H

typedef struct _php_parallel_runtime_t {
    pthread_t                        thread;
    php_parallel_monitor_t          *monitor;
    zend_string                     *bootstrap;
    struct {
        zend_bool                   *interrupt;
    } child;
    struct {
        void                        *server;
    } parent;
    zend_llist                       schedule;
    zend_object                      std;
} php_parallel_runtime_t;

static zend_always_inline php_parallel_runtime_t* php_parallel_runtime_fetch(zend_object *o) {
    return (php_parallel_runtime_t*) (((char*) o) - XtOffsetOf(php_parallel_runtime_t, std));
}

static zend_always_inline php_parallel_runtime_t* php_parallel_runtime_from(zval *z) {
    return php_parallel_runtime_fetch(Z_OBJ_P(z));
}

extern zend_class_entry* php_parallel_runtime_ce;

php_parallel_runtime_t* php_parallel_runtime_construct(zend_string *bootstrap);

PHP_MINIT_FUNCTION(PARALLEL_RUNTIME);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_RUNTIME);
#endif
