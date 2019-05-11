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
#ifndef HAVE_PARALLEL_STRINGS
#define HAVE_PARALLEL_STRINGS

#include "parallel.h"

TSRM_TLS HashTable php_parallel_strings;

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
} php_parallel_strings_interned = {PTHREAD_MUTEX_INITIALIZER};

#define PSI(e) php_parallel_strings_interned.e

zend_string* php_parallel_string(zend_string *source) {
    zend_string *local =
        (zend_string*)
            zend_hash_find_ptr(
                &php_parallel_strings, source);

    if (!local) {
        local = zend_string_dup(source, 0);

        zend_hash_add_ptr(
            &php_parallel_strings,
            local, local);
    }

    return local;
}

static void php_parallel_strings_release(zval *zv) {
    zend_string_release(Z_PTR_P(zv));
}

zend_string* php_parallel_string_interned(zend_string *string) {
    zend_string *cached;

    pthread_mutex_lock(&PSI(mutex));

    cached =
        (zend_string*)
            zend_hash_index_find_ptr(
                &PSI(table), (zend_ulong) string);

    if (!cached) {
        cached = zend_hash_index_add_mem(&PSI(table),
                    (zend_ulong) string,
                    string, _ZSTR_STRUCT_SIZE(ZSTR_LEN(string)));

        GC_TYPE_INFO(cached) =
            IS_STRING | ((IS_STR_INTERNED | IS_STR_PERMANENT) << GC_FLAGS_SHIFT);

        zend_string_hash_val(cached);
    }

    pthread_mutex_unlock(&PSI(mutex));

    return cached;
}

static void php_parallel_strings_interned_dtor(zval *zv) {
    free(Z_PTR_P(zv));
}

PHP_MINIT_FUNCTION(PARALLEL_STRINGS)
{
    zend_hash_init(&PSI(table), 32, NULL, php_parallel_strings_interned_dtor, 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_STRINGS)
{
    zend_hash_destroy(&PSI(table));

    return SUCCESS;
}

PHP_RINIT_FUNCTION(PARALLEL_STRINGS)
{
    zend_hash_init(
        &php_parallel_strings,
        32, NULL,
        php_parallel_strings_release, 0);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_STRINGS)
{
    zend_hash_destroy(&php_parallel_strings);

    return SUCCESS;
}
#endif
