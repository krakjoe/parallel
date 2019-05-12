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
#ifndef HAVE_PARALLEL_COPY
#define HAVE_PARALLEL_COPY

#include "parallel.h"

#include "php_streams.h"
#include "php_network.h"

TSRM_TLS struct {
    HashTable uncopied;
    HashTable used;
    HashTable scope;
} php_parallel_copy_globals;

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
} php_parallel_copy_cache;

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
    HashTable       index;
} php_parallel_copy_strings = {PTHREAD_MUTEX_INITIALIZER};

#define PCG(e) php_parallel_copy_globals.e
#define PCC(e) php_parallel_copy_cache.e
#define PCS(e) php_parallel_copy_strings.e

static const uint32_t php_parallel_copy_uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static void php_parallel_copy_string_dtor(zval *zv) {
    free(Z_PTR_P(zv));
}

static void php_parallel_copy_cache_dtor(zval *zv) {
    zend_op_array *cached = 
        (zend_op_array*) Z_FUNC_P(zv);

    if (cached->static_variables) {
        php_parallel_copy_hash_dtor(cached->static_variables, 1);
    }

    pefree(cached, 1);
}

static void php_parallel_copy_uncopied_dtor(zval *zv) {
    zend_op_array *uncopied = 
        (zend_op_array*) Z_FUNC_P(zv);
    zend_string *key =
        (zend_string*) zend_hash_index_find_ptr(
            &PCG(used), (zend_ulong) uncopied);

    if (key) {
        if (zend_hash_exists(EG(function_table), key)) {
            dtor_func_t dtor = /* temporarily remove destructor */
                EG(function_table)->pDestructor;

            EG(function_table)->pDestructor = NULL;

            zend_hash_del(EG(function_table), key);

            EG(function_table)->pDestructor = dtor;
        }
    }

    if (uncopied->static_variables) {
        php_parallel_copy_hash_dtor(uncopied->static_variables, 0);
    }

    pefree(uncopied, 0);
}

static zend_always_inline zend_class_entry* php_parallel_copy_scope(zend_class_entry *class) { /* {{{ */
    zend_class_entry *scope =
        (zend_class_entry*)
            zend_hash_index_find_ptr(&PCG(scope), (zend_ulong) class);

    if (scope) {
        return scope;
    }

    scope = zend_lookup_class(class->name);

    if (!scope) {
        return NULL;
    }

    return zend_hash_index_update_ptr(&PCG(scope), (zend_ulong) class, scope);
} /* }}} */

static zend_always_inline void php_parallel_copy_resource(zval *dest, zval *source) {
    zend_resource *resource = Z_RES_P(source);
#ifndef _WIN32
    if (resource->type == php_file_le_stream() || resource->type == php_file_le_pstream()) {
        int fd;
        php_stream *stream = zend_fetch_resource2_ex(
                                source, "stream",
                                php_file_le_stream(),
                                php_file_le_pstream());

        if (stream) {
            if (php_stream_cast(stream, PHP_STREAM_AS_FD, (void*)&fd, 0) == SUCCESS) {
                ZVAL_LONG(dest, fd);
                return;
            }
        }
    }
#endif
    ZVAL_NULL(dest);
}

static zend_always_inline HashTable* php_parallel_copy_hash_permanent(HashTable *source) {
    HashTable *ht = php_parallel_copy_mem(source, sizeof(HashTable), 1);
    uint32_t idx;

    GC_SET_REFCOUNT(ht, 2);
    GC_SET_PERSISTENT_TYPE(ht, GC_ARRAY);
    GC_ADD_FLAGS(ht, IS_ARRAY_IMMUTABLE);

    ht->pDestructor = PARALLEL_ZVAL_DTOR;

#if PHP_VERSION_ID < 70300
    ht->u.flags |= HASH_FLAG_APPLY_PROTECTION|HASH_FLAG_PERSISTENT;
#endif

    ht->u.flags |= HASH_FLAG_STATIC_KEYS;
    if (ht->nNumUsed == 0) {
#if PHP_VERSION_ID >= 70400
        ht->u.flags = HASH_FLAG_UNINITIALIZED;
#else
        ht->u.flags &= ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED);
#endif
        ht->nNextFreeElement = 0;
        ht->nTableMask = HT_MIN_MASK;
        HT_SET_DATA_ADDR(ht, &php_parallel_copy_uninitialized_bucket);
        return ht;
    }

    ht->nNextFreeElement = 0;
    ht->nInternalPointer = 0;
    HT_SET_DATA_ADDR(ht, php_parallel_copy_mem(HT_GET_DATA_ADDR(ht), HT_USED_SIZE(ht), 1));
    for (idx = 0; idx < ht->nNumUsed; idx++) {
        Bucket *p = ht->arData + idx;
        if (Z_TYPE(p->val) == IS_UNDEF) continue;

        if (p->key) {
            p->key = php_parallel_copy_string_interned(p->key);
            ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
        } else if ((zend_long) p->h >= (zend_long) ht->nNextFreeElement) {
            ht->nNextFreeElement = p->h + 1;
        }

        if (Z_OPT_REFCOUNTED(p->val)) {
            PARALLEL_ZVAL_COPY(&p->val, &p->val, 1);
        }
    }

    return ht;
}

static zend_always_inline HashTable* php_parallel_copy_hash_thread(HashTable *source) {
    HashTable *ht = php_parallel_copy_mem(source, sizeof(HashTable), 0);

    GC_SET_REFCOUNT(ht, 1);
    GC_DEL_FLAGS(ht, IS_ARRAY_IMMUTABLE);

    GC_TYPE_INFO(ht) = GC_ARRAY;

#if PHP_VERSION_ID < 70300
    ht->u.flags &= ~HASH_FLAG_PERSISTENT;
#endif

    ht->pDestructor = ZVAL_PTR_DTOR;

    if (ht->nNumUsed == 0) {
        HT_SET_DATA_ADDR(ht, &php_parallel_copy_uninitialized_bucket);
        return ht;
    }

    HT_SET_DATA_ADDR(ht, emalloc(HT_SIZE(ht)));
    memcpy(
        HT_GET_DATA_ADDR(ht),
        HT_GET_DATA_ADDR(source),
        HT_HASH_SIZE(ht->nTableMask));

    if (ht->u.flags & HASH_FLAG_STATIC_KEYS) {
        Bucket *p = ht->arData,
        *q = source->arData,
        *p_end = p + ht->nNumUsed;
        for (; p < p_end; p++, q++) {
            *p = *q;
            if (Z_OPT_REFCOUNTED(p->val)) {
                PARALLEL_ZVAL_COPY(&p->val, &p->val, 0);
            }
        }
    } else {
        Bucket *p = ht->arData,
        *q = source->arData,
        *p_end = p + ht->nNumUsed;
        for (; p < p_end; p++, q++) {
            if (Z_TYPE(q->val) == IS_UNDEF) {
                ZVAL_UNDEF(&p->val);
                continue;
            }

            p->val = q->val;
            p->h = q->h;
            if (q->key) {
                p->key = php_parallel_copy_string(q->key, 0);
            } else {
                p->key = NULL;
            }

            if (Z_OPT_REFCOUNTED(p->val)) {
                PARALLEL_ZVAL_COPY(&p->val, &p->val, 0);
            }
        }
    }

    return ht;
}

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_hash_permanent(source);
    }
    return php_parallel_copy_hash_thread(source);
}

void php_parallel_copy_hash_dtor(HashTable *table, zend_bool persistent) {
    if (GC_DELREF(table) == (persistent ? 1 : 0)) {
        Bucket *p = table->arData,
               *end = p + table->nNumUsed;

        for (p = table->arData, end = p + table->nNumUsed; p < end; p++) {
            if (Z_ISUNDEF(p->val)) {
                continue;
            }

            if (p->key && !ZSTR_IS_INTERNED(p->key)) {
                if (GC_DELREF(p->key) == 0) {
                    pefree(p->key, persistent);
                }
            }

            if (Z_OPT_REFCOUNTED(p->val)) {
                php_parallel_copy_zval_dtor(&p->val);
            }
        }

        if (HT_GET_DATA_ADDR(table) != (void*) &php_parallel_copy_uninitialized_bucket) {
            pefree(HT_GET_DATA_ADDR(table), persistent);
        }

        pefree(table, persistent);
    }
}

static zend_always_inline void php_parallel_copy_closure_init_run_time_cache(zend_closure_t *closure) {
    void *rtc;

#ifdef ZEND_ACC_HEAP_RT_CACHE
    closure->func.op_array.fn_flags |= ZEND_ACC_HEAP_RT_CACHE;
#else
    closure->func.op_array.fn_flags |= ZEND_ACC_NO_RT_ARENA;
#endif

#ifdef ZEND_MAP_PTR_SET
    {
        rtc = emalloc(sizeof(void*) + closure->func.op_array.cache_size);

        ZEND_MAP_PTR_INIT(closure->func.op_array.run_time_cache, rtc);

        rtc = (char*)rtc + sizeof(void*);

        ZEND_MAP_PTR_SET(closure->func.op_array.run_time_cache, rtc);
    }
#else
    closure->func.op_array.run_time_cache = rtc = emalloc(closure->func.op_array.cache_size);
#endif

    memset(rtc, 0, closure->func.op_array.cache_size);
}

static zend_always_inline void php_parallel_copy_closure(zval *destination, zval *source, zend_bool persistent) { /* {{{ */
    zend_closure_t *closure =
        (zend_closure_t*) Z_OBJ_P(source);
    zend_closure_t *copy =
        (zend_closure_t*)
            php_parallel_copy_mem(
                closure, sizeof(zend_closure_t), persistent);

    if (persistent) {
        zend_function  *function;

        if (copy->func.op_array.refcount) {
            function =
                php_parallel_cache_function(&copy->func);
        } else {
            function = (zend_function*) &copy->func;
        }

        memcpy(
            &copy->func,
            php_parallel_copy_function(function, 1),
            sizeof(zend_op_array));

        copy->func.common.fn_flags |= ZEND_ACC_CLOSURE;
    } else {
        zend_object_std_init(&copy->std, copy->std.ce);

        memcpy(
            &copy->func,
            php_parallel_copy_function(&copy->func, 0),
            sizeof(zend_op_array));

        if (copy->func.op_array.static_variables) {
            copy->func.op_array.static_variables =
                php_parallel_copy_hash_ctor(copy->func.op_array.static_variables, 0);
        }

#ifdef ZEND_MAP_PTR_INIT
        ZEND_MAP_PTR_INIT(copy->func.op_array.static_variables_ptr, &copy->func.op_array.static_variables);
#endif

        php_parallel_copy_closure_init_run_time_cache(copy);

#if PHP_VERSION_ID < 70300
        copy->func.common.prototype = (void*) copy;
#endif

        if (copy->called_scope &&
            copy->called_scope->type == ZEND_USER_CLASS) {
            copy->called_scope =
                php_parallel_copy_scope(copy->called_scope);
        }

        ZVAL_UNDEF(&copy->this_ptr);
    }

    ZVAL_OBJ(destination, &copy->std);

    destination->u2.extra = persistent;
} /* }}} */

static zend_always_inline zend_string* php_parallel_copy_string_ex(zend_string *source, zend_bool persistent) { /* {{{ */
    zend_string *dest = zend_string_alloc(ZSTR_LEN(source), persistent);

    memcpy(ZSTR_VAL(dest),
           ZSTR_VAL(source),
           ZSTR_LEN(source));

    ZSTR_VAL(dest)[ZSTR_LEN(dest)] = 0;

    ZSTR_LEN(dest) = ZSTR_LEN(source);
    ZSTR_H(dest)   = ZSTR_H(source);

    return dest;
} /* }}} */

zend_string* php_parallel_copy_string_interned(zend_string *source) { /* {{{ */
    zend_string *dest;

    pthread_mutex_lock(&PCS(mutex));

    if (!zend_hash_index_exists(&PCS(index), (zend_ulong) source)) {

        if (!(dest = zend_hash_index_find_ptr(&PCS(table), (zend_ulong) source))) {

            dest = php_parallel_copy_string_ex(source, 1);

            zend_string_hash_val(dest);

            zend_hash_index_add_ptr(&PCS(table), (zend_ulong) source, dest);

            GC_TYPE_INFO(dest) =
                IS_STRING |
                ((IS_STR_INTERNED | IS_STR_PERMANENT) << GC_FLAGS_SHIFT);

            zend_hash_index_add_empty_element(&PCS(index), (zend_ulong) dest);
        }
    } else {
        dest = source;
    }

    pthread_mutex_unlock(&PCS(mutex));

    return dest;
} /* }}} */

zend_string* php_parallel_copy_string(zend_string *source, zend_bool persistent) { /* {{{ */
    if (ZSTR_IS_INTERNED(source)) {
        return php_parallel_copy_string_interned(source);
    }

    return php_parallel_copy_string_ex(source, persistent);
} /* }}} */


void php_parallel_copy_zval_ctor(zval *dest, zval *source, zend_bool persistent) {
    switch (Z_TYPE_P(source)) {
        case IS_NULL:
        case IS_TRUE:
        case IS_FALSE:
        case IS_LONG:
        case IS_DOUBLE:
        case IS_UNDEF:
            if (source != dest) {
                *dest = *source;
            }
        break;

        case IS_STRING:
            ZVAL_STR(dest, php_parallel_copy_string(Z_STR_P(source), persistent));
        break;

        case IS_ARRAY:
            ZVAL_ARR(dest, php_parallel_copy_hash_ctor(Z_ARRVAL_P(source), persistent));
        break;

        case IS_OBJECT:
            if (Z_OBJCE_P(source) == zend_ce_closure) {
                php_parallel_copy_closure(dest, source, persistent);
            } else {
                ZVAL_TRUE(dest);
            }
        break;

        case IS_RESOURCE:
            if (php_parallel_check_resource(source)) {
                php_parallel_copy_resource(dest, source);
                break;
            }

        default:
            ZVAL_BOOL(dest, zend_is_true(source));
    }
}

static zend_always_inline zend_function* php_parallel_copy_function_permanent(const zend_function *function) { /* {{{ */
    zend_op_array *copy;

#ifdef ZEND_ACC_IMMUTABLE
    /*
    * The function must already be immutable either because it's in opcache
    * or because it's in l2
    */
    ZEND_ASSERT(function->common.fn_flags & ZEND_ACC_IMMUTABLE);
#endif

    /*
    * If this is not a closure, it does not need to be modified
    */
    if (!(function->common.fn_flags & ZEND_ACC_CLOSURE)) {
        php_parallel_dependencies_store(function);

        return (zend_function*) function;
    }

    pthread_mutex_lock(&PCC(mutex));

    if ((copy = zend_hash_index_find_ptr(&PCC(table), (zend_ulong) function->op_array.opcodes))) {
        goto _php_parallel_copied_function_permanent;
    }

    /*
    * Buffer the function (exclusively) in process wide memory
    */
    copy = zend_hash_index_add_mem(
            &PCC(table),
            (zend_ulong) function->op_array.opcodes,
            (void*) function, sizeof(zend_op_array));

    copy->fn_flags &= ~ZEND_ACC_CLOSURE;

    /*
    * Must buffer statics because the closure may be destroyed
    */
    if (copy->static_variables) {
        copy->static_variables = 
            php_parallel_copy_hash_ctor(copy->static_variables, 1);
    }

    /*
    * Create dependency tree (nested lambdas currently)
    */
    php_parallel_dependencies_store((zend_function*)copy);

_php_parallel_copied_function_permanent:

    pthread_mutex_unlock(&PCC(mutex));

    return (zend_function*) copy;
} /* }}} */

static zend_always_inline zend_function* php_parallel_copy_function_thread(const zend_function *function) { /* {{{ */
    zend_op_array *copy;

    /*
    * Unbuffer once per thread
    */
    if ((copy = zend_hash_index_find_ptr(&PCG(uncopied), (zend_ulong) function->op_array.opcodes))) {
        return (zend_function*) copy;
    }

    copy = php_parallel_copy_mem((void*) function, sizeof(zend_op_array), 0);

    if (copy->static_variables) {
        /* TODO see if this can be eliminated */
        copy->static_variables =
            php_parallel_copy_hash_ctor(copy->static_variables, 0);
        GC_ADD_FLAGS(copy->static_variables, IS_ARRAY_IMMUTABLE);
    }

#ifdef ZEND_MAP_PTR_NEW
    ZEND_MAP_PTR_INIT(copy->static_variables_ptr, &copy->static_variables);
    ZEND_MAP_PTR_NEW(copy->run_time_cache);
#else
    copy->run_time_cache = NULL;
#endif

    /*
    * It may not have changed, possibly check ZEND_ACC_IMMUTABLE on class entry
    */
    if (copy->scope) {
        copy->scope =
            php_parallel_copy_scope(copy->scope);
    }

    /*
    * Must load depdendencies
    */
    php_parallel_dependencies_load(function);

    return zend_hash_index_add_ptr(&PCG(uncopied), (zend_ulong) function->op_array.opcodes, copy);
} /* }}} */

zend_function* php_parallel_copy_function(const zend_function *function, zend_bool persistent) { /* {{{ */
    if (persistent) {
        return php_parallel_copy_function_permanent(function);
    }
    return php_parallel_copy_function_thread(function);
} /* }}} */

void php_parallel_copy_function_use(zend_string *key, zend_function *function) { /* {{{ */
    function = php_parallel_copy_function(function, 0);

    if (zend_hash_add_ptr(EG(function_table), key, function)) {
        zend_hash_index_add_ptr(
            &PCG(used),
            (zend_ulong) function, key);
    }
} /* }}} */

PHP_RINIT_FUNCTION(PARALLEL_COPY)
{
    zend_hash_init(&PCG(uncopied),  32, NULL, php_parallel_copy_uncopied_dtor, 0);
    zend_hash_init(&PCG(used),      32, NULL, NULL, 0);
    zend_hash_init(&PCG(scope),     32, NULL, NULL, 0);

    PHP_RINIT(PARALLEL_CHECK)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_RINIT(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_COPY)
{
    zend_hash_destroy(&PCG(uncopied));
    zend_hash_destroy(&PCG(used));
    zend_hash_destroy(&PCG(scope));

    PHP_RSHUTDOWN(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_RSHUTDOWN(PARALLEL_CHECK)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_COPY_STRINGS)
{
    zend_hash_init(&PCS(table), 32, NULL, php_parallel_copy_string_dtor, 1);
    zend_hash_init(&PCS(index), 32, NULL, NULL, 1);

    return SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_COPY)
{
    pthread_mutexattr_t attributes;

    pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

    pthread_mutex_init(&PCC(mutex), &attributes);
    pthread_mutexattr_destroy(&attributes);

    zend_hash_init(&PCC(table), 32, NULL, php_parallel_copy_cache_dtor, 1);

    PHP_MINIT(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_CACHE)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_COPY_STRINGS)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(PARALLEL_COPY_STRINGS)
{
    zend_hash_destroy(&PCS(table));
    zend_hash_destroy(&PCS(index));

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_COPY)
{
    zend_hash_destroy(&PCC(table));
    pthread_mutex_destroy(&PCC(mutex));

    PHP_MSHUTDOWN(PARALLEL_CACHE)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_COPY_STRINGS)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}
#endif
