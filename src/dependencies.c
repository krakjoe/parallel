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
} php_parallel_dependencies_map;

#define PDM(e) php_parallel_dependencies_map.e

TSRM_TLS struct {
    HashTable activated;
} php_parallel_dependencies_globals;

#define PDG(e) php_parallel_dependencies_globals.e

/* {{{ */
static zend_always_inline void php_parallel_dependencies_load_globals_vars(const zend_function *function) {
    zend_string **variables = function->op_array.vars;
    int it = 0,
        end = function->op_array.last_var;

    while (it < end) {
        zend_is_auto_global(variables[it]);
        it++;
    }
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_dependencies_load_globals_literals(const zend_function *function) {
    zval *literals = function->op_array.literals;
    int it = 0,
        end = function->op_array.last_literal;

    while (it < end) {
        if (Z_TYPE(literals[it]) == IS_STRING) {
            zend_is_auto_global(Z_STR(literals[it]));
        }
        it++;
    }
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_dependencies_load_globals(const zend_function *function) {
    if (zend_hash_index_exists(&PDG(activated), (zend_ulong) function->op_array.opcodes)) {
        return;
    }

    php_parallel_dependencies_load_globals_vars(function);
    php_parallel_dependencies_load_globals_literals(function);

    zend_hash_index_add_empty_element(&PDG(activated), (zend_ulong) function->op_array.opcodes);
} /* }}} */


void php_parallel_dependencies_store(const zend_function *function) { /* {{{ */
    HashTable dependencies;

    pthread_mutex_lock(&PDM(mutex));

    if (zend_hash_index_exists(&PDM(table), (zend_ulong) function->op_array.opcodes)) {
        pthread_mutex_unlock(&PDM(mutex));
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

                dependency = php_parallel_copy_function(dependency, 1);

                if (dependencies.nNumUsed == 0) {
                    zend_hash_init(&dependencies, 8, NULL, NULL, 1);
                }

                zend_hash_add_ptr(
                    &dependencies,
                    php_parallel_string(key),
                    dependency);

                php_parallel_dependencies_store(dependency);
            }
            opline++;
        }
    }

    zend_hash_index_add_mem(
        &PDM(table),
        (zend_ulong) function->op_array.opcodes,
        &dependencies, sizeof(HashTable));

    pthread_mutex_unlock(&PDM(mutex));
} /* }}} */

void php_parallel_dependencies_load(const zend_function *function) { /* {{{ */
    HashTable *dependencies;
    zend_string *key;
    zend_function *dependency;

    php_parallel_dependencies_load_globals(function);

    pthread_mutex_lock(&PDM(mutex));
    dependencies = zend_hash_index_find_ptr(
        &PDM(table), (zend_ulong) function->op_array.opcodes);
    pthread_mutex_unlock(&PDM(mutex));

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

PHP_RINIT_FUNCTION(PARALLEL_DEPENDENCIES)
{
    zend_hash_init(&PDG(activated), 32, NULL, NULL, 0);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_DEPENDENCIES)
{
    zend_hash_destroy(&PDG(activated));

    return SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_DEPENDENCIES)
{
    pthread_mutexattr_t attributes;

    pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

    pthread_mutex_init(&PDM(mutex), &attributes);

    pthread_mutexattr_destroy(&attributes);

    zend_hash_init(&PDM(table), 32, NULL, php_parallel_dependencies_dtor, 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_DEPENDENCIES)
{
    zend_hash_destroy(&PDM(table));
    pthread_mutex_destroy(&PDM(mutex));

    return SUCCESS;
}
#endif
