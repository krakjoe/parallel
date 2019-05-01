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

typedef struct _php_parallel_runtime_functions_t {
    HashTable               lambdas;
    HashTable               functions;
} php_parallel_runtime_functions_t;

typedef struct _php_parallel_runtime_t {
    pthread_t                        thread;
    php_parallel_monitor_t          *monitor;
    zend_string                     *bootstrap;
    php_parallel_runtime_functions_t functions;
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

void php_parallel_runtime_functions_setup(php_parallel_runtime_functions_t *functions, zend_bool thread);
void php_parallel_runtime_function_push(php_parallel_runtime_t *runtime, zend_string *name, const zend_function *function, zend_bool lambda);
void php_parallel_runtime_functions_update(php_parallel_runtime_t *runtime, php_parallel_runtime_functions_t *functions);
void php_parallel_runtime_functions_finish(php_parallel_runtime_functions_t *functions);

void         php_parallel_runtime_startup();
void         php_parallel_runtime_shutdown();

void*        php_parallel_runtime(void *arg);

extern zend_class_entry* php_parallel_runtime_ce;
#endif
