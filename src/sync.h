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
#ifndef HAVE_PARALLEL_SYNC_H
#define HAVE_PARALLEL_SYNC_H

extern zend_class_entry *php_parallel_sync_ce;

typedef struct _php_parallel_sync_t {
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    zval            value;
    uint32_t        refcount;
} php_parallel_sync_t;

typedef struct _php_parallel_sync_object_t {
    php_parallel_sync_t *sync;
    zend_object std;
} php_parallel_sync_object_t;

static zend_always_inline php_parallel_sync_object_t* php_parallel_sync_object_fetch(zend_object *o) {
    return (php_parallel_sync_object_t*) (((char*) o) - XtOffsetOf(php_parallel_sync_object_t, std));
}

static zend_always_inline php_parallel_sync_object_t* php_parallel_sync_object_from(zval *z) {
    return php_parallel_sync_object_fetch(Z_OBJ_P(z));
}

php_parallel_sync_t*   php_parallel_sync_copy(php_parallel_sync_t *sync);
void                   php_parallel_sync_release(php_parallel_sync_t *sync);

PHP_MINIT_FUNCTION(PARALLEL_SYNC);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_SYNC);
#endif
