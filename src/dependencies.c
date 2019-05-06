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
#ifndef HAVE_PARALLEL_DEPENDENCIES
#define HAVE_PARALLEL_DEPENDENCIES

#include "parallel.h"

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
} php_parallel_dependencies_globals;

#define PCD(e) php_parallel_dependencies_globals.e

void php_parallel_dependencies_store(const zend_function *function) { /* {{{ */
    HashTable dependencies;

    pthread_mutex_lock(&PCD(mutex));

    if (zend_hash_index_exists(&PCD(table), (zend_ulong) function->op_array.opcodes)) {
        pthread_mutex_unlock(&PCD(mutex));
        return;
    }

    memset(&dependencies, 0, sizeof(HashTable));
    {
        zend_op *opline = function->op_array.opcodes,
                *end = opline + function->op_array.last;

        while (opline < end) {
            if (opline->opcode == ZEND_DECLARE_LAMBDA_FUNCTION) {
                zend_string   *key;
                zend_function *dependency;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, &key, &dependency);

                if (dependency->op_array.refcount) {
                    dependency = php_parallel_cache_function(dependency);
                }

                if (dependencies.nNumUsed == 0) {
                    zend_hash_init(&dependencies, 8, NULL, NULL, 1);
                }

                zend_hash_add_ptr(
                    &dependencies,
                    php_parallel_string(key), dependency);

                php_parallel_dependencies_store(dependency);
            }
            opline++;
        }
    }

    zend_hash_index_add_mem(
        &PCD(table),
        (zend_ulong) function->op_array.opcodes,
        &dependencies, sizeof(HashTable));

    pthread_mutex_unlock(&PCD(mutex));
} /* }}} */

void php_parallel_dependencies_load(const zend_function *function) { /* {{{ */
    HashTable *dependencies;
    zend_string *key;
    zend_function *dependency;

    pthread_mutex_lock(&PCD(mutex));
    dependencies = zend_hash_index_find_ptr(
        &PCD(table), (zend_ulong) function->op_array.opcodes);
    pthread_mutex_unlock(&PCD(mutex));

    /* read only table */

    if (!dependencies || !dependencies->nNumUsed) {
        return;
    }

    ZEND_HASH_FOREACH_STR_KEY_PTR(dependencies, key, dependency) {
        if (!zend_hash_exists(EG(function_table), key)) {
            php_parallel_copy_function_use(key, dependency);
        }
    } ZEND_HASH_FOREACH_END();
} /* }}} */

static void php_parallel_dependencies_dtor(zval *zv) { /* {{{ */
    zend_hash_destroy(Z_PTR_P(zv));
    pefree(Z_PTR_P(zv), 1);
} /* }}} */

void php_parallel_dependencies_startup(void) { /* {{{ */
    pthread_mutexattr_t attributes;

    pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

    pthread_mutex_init(&PCD(mutex), &attributes);

    pthread_mutexattr_destroy(&attributes);

    zend_hash_init(&PCD(table), 32, NULL, php_parallel_dependencies_dtor, 1);
} /* }}} */

void php_parallel_dependencies_shutdown(void) { /* {{{ */
    zend_hash_destroy(&PCD(table));
    pthread_mutex_destroy(&PCD(mutex));
} /* }}} */
#endif
