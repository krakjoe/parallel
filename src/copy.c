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
    HashTable scope;
    zval      unavailable;
} php_parallel_copy_globals;

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
} php_parallel_copy_strings = {PTHREAD_MUTEX_INITIALIZER};

zend_class_entry* php_parallel_copy_type_unavailable_ce;
zend_class_entry* php_parallel_copy_object_unavailable_ce;

#define PCG(e) php_parallel_copy_globals.e
#define PCS(e) php_parallel_copy_strings.e

static const uint32_t php_parallel_copy_uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static void php_parallel_copy_string_free(zval *zv) {
    free(Z_PTR_P(zv));
}

static zend_always_inline zend_string* php_parallel_copy_string_ex(zend_string *source, zend_bool persistent) {
    zend_string *dest = zend_string_alloc(ZSTR_LEN(source), persistent);

    memcpy(ZSTR_VAL(dest),
           ZSTR_VAL(source),
           ZSTR_LEN(source));

    ZSTR_VAL(dest)[ZSTR_LEN(dest)] = 0;

    ZSTR_LEN(dest) = ZSTR_LEN(source);
    ZSTR_H(dest)   = ZSTR_H(source);

    return dest;
}

zend_string* php_parallel_copy_string_interned(zend_string *source) {
    zend_string *dest;

    pthread_mutex_lock(&PCS(mutex));

    if (!(dest = zend_hash_find_ptr(&PCS(table), source))) {

        dest = php_parallel_copy_string_ex(source, 1);

        zend_string_hash_val(dest);

        GC_TYPE_INFO(dest) =
            IS_STRING |
            ((IS_STR_INTERNED | IS_STR_PERMANENT) << GC_FLAGS_SHIFT);

        zend_hash_add_ptr(&PCS(table), dest, dest);
    }

    pthread_mutex_unlock(&PCS(mutex));

    return dest;
}

static zend_always_inline void php_parallel_copy_string_dtor(zend_string *source, zend_bool persistent) {
    if (ZSTR_IS_INTERNED(source)) {
        return;
    }

    if (GC_DELREF(source) == 0) {
        pefree(source, persistent);
    }
}

static zend_always_inline zend_string* php_parallel_copy_string_ctor(zend_string *source, zend_bool persistent) {
    if (ZSTR_IS_INTERNED(source)) {
        return php_parallel_copy_string_interned(source);
    }

    return php_parallel_copy_string_ex(source, persistent);
}

zend_string* php_parallel_copy_string(zend_string *source, zend_bool persistent) {
    return php_parallel_copy_string_ctor(source, persistent);
}

zend_class_entry* php_parallel_copy_scope(zend_class_entry *class) {
    zend_class_entry *scope, *exists_ce;

#ifdef ZEND_ACC_IMMUTABLE
    if (class->ce_flags & ZEND_ACC_IMMUTABLE) {
        exists_ce = zend_lookup_class_ex(class->name, NULL, ZEND_FETCH_CLASS_NO_AUTOLOAD);
        if (exists_ce) {
            return class;
        }
    }
#endif

    if ((scope = zend_hash_index_find_ptr(&PCG(scope), (zend_ulong) class))) {
        return scope;
    }

    scope = zend_lookup_class(class->name);

    if (!scope) {
        return php_parallel_copy_type_unavailable_ce;
    }

    return zend_hash_index_update_ptr(&PCG(scope), (zend_ulong) class, scope);
}

static zend_always_inline zend_long php_parallel_copy_resource_ctor(zend_resource *source, zend_bool persistent) {
#ifndef _WIN32
    if (source->type == php_file_le_stream() ||
        source->type == php_file_le_pstream()) {
        int fd;

        if (php_stream_cast((php_stream*) source->ptr, PHP_STREAM_AS_FD, (void*)&fd, 0) == SUCCESS) {
            return (zend_long) fd;
        }
    }
#endif
    return -1;
}

static void php_parallel_copy_zval_persistent(
                zval *dest, zval *source,
                zend_string* (*php_parallel_copy_string_func)(zend_string*),
                void* (*php_parallel_copy_memory_func)(void *source, zend_long size)) {
    if (Z_TYPE_P(source) == IS_ARRAY) {
        ZVAL_ARR(dest,
            php_parallel_copy_hash_persistent(
                Z_ARRVAL_P(source),
                php_parallel_copy_string_func,
                php_parallel_copy_memory_func));
    } else if (Z_TYPE_P(source) == IS_REFERENCE) {
        ZVAL_REF(dest,
            php_parallel_copy_memory_func(
                Z_REF_P(source), sizeof(zend_reference)));
        php_parallel_copy_zval_persistent(
            Z_REFVAL_P(dest), Z_REFVAL_P(source),
            php_parallel_copy_string_func,
            php_parallel_copy_memory_func);
        GC_SET_REFCOUNT(Z_REF_P(dest), 1);
        GC_ADD_FLAGS(Z_REF_P(dest), GC_IMMUTABLE);
    } else if (Z_TYPE_P(source) == IS_STRING) {
        ZVAL_STR(dest, php_parallel_copy_string_func(Z_STR_P(source)));
    } else {
        PARALLEL_ZVAL_COPY(dest, source, 1);
    }
}

static zend_always_inline HashTable* php_parallel_copy_hash_persistent_inline(
                HashTable *source,
                zend_string* (*php_parallel_copy_string_func)(zend_string*),
                void* (*php_parallel_copy_memory_func)(void *source, zend_long size)) {
    HashTable *ht = php_parallel_copy_memory_func(source, sizeof(HashTable));
    uint32_t idx;

    GC_SET_REFCOUNT(ht, 2);
    GC_SET_PERSISTENT_TYPE(ht, GC_ARRAY);
    GC_ADD_FLAGS(ht, IS_ARRAY_IMMUTABLE);

    ht->pDestructor = ZVAL_PTR_DTOR;

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

    HT_SET_DATA_ADDR(ht, php_parallel_copy_memory_func(HT_GET_DATA_ADDR(ht), HT_USED_SIZE(ht)));
    for (idx = 0; idx < ht->nNumUsed; idx++) {
        Bucket *p = ht->arData + idx;

        if (Z_TYPE(p->val) == IS_UNDEF)
            continue;

        if (p->key) {
            p->key = php_parallel_copy_string_interned(p->key);
            ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
        } else if ((zend_long) p->h >= (zend_long) ht->nNextFreeElement) {
            ht->nNextFreeElement = p->h + 1;
        }

        if (Z_OPT_REFCOUNTED(p->val)) {
            php_parallel_copy_zval_persistent(
                &p->val,
                &p->val,
                php_parallel_copy_string_func,
                php_parallel_copy_memory_func);
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
                p->key = php_parallel_copy_string_ctor(q->key, 0);
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

static void* php_parallel_copy_mem_persistent(void *source, zend_long size) {
    return php_parallel_copy_mem(source, size, 1);
}

static zend_string* php_parallel_copy_string_persistent(zend_string *string) {
    return php_parallel_copy_string_ctor(string, 1);
}

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_hash_persistent_inline(
                source,
                php_parallel_copy_string_persistent,
                php_parallel_copy_mem_persistent);
    }
    return php_parallel_copy_hash_thread(source);
}

HashTable *php_parallel_copy_hash_persistent(HashTable *source,
            zend_string* (*php_parallel_copy_string_func)(zend_string*),
            void* (*php_parallel_copy_memory_func)(void *source, zend_long size)) {
        return php_parallel_copy_hash_persistent_inline(source,
                php_parallel_copy_string_func,
                php_parallel_copy_memory_func);
}

void php_parallel_copy_hash_dtor(HashTable *table, zend_bool persistent) {
    if (GC_DELREF(table) == (persistent ? 1 : 0)) {
        Bucket *p = table->arData,
               *end = p + table->nNumUsed;

        if (!persistent) {
            GC_REMOVE_FROM_BUFFER(table);
            GC_TYPE_INFO(table) =
#ifdef GC_WHITE
                IS_NULL | (GC_WHITE << 16);
#else
                IS_NULL;
#endif
        }

        if (HT_HAS_STATIC_KEYS_ONLY(table)) {
            while (p < end) {
                if (Z_OPT_REFCOUNTED(p->val)) {
                    PARALLEL_ZVAL_DTOR(&p->val);
                }
                p++;
            }
        } else {
            while (p < end) {
                if (Z_ISUNDEF(p->val)) {
                    p++;
                    continue;
                }

                if (p->key) {
                    php_parallel_copy_string_dtor(
                        p->key,
                        GC_FLAGS(p->key) & IS_STR_PERSISTENT);
                }

                if (Z_OPT_REFCOUNTED(p->val)) {
                    php_parallel_copy_zval_dtor(&p->val);
                }

                p++;
            }
        }

        if (HT_GET_DATA_ADDR(table) != (void*) &php_parallel_copy_uninitialized_bucket) {
            pefree(HT_GET_DATA_ADDR(table), persistent);
        }

        pefree(table, persistent);
    }
}

static zend_always_inline zend_object* php_parallel_copy_closure_persistent(zend_object *source) {
    zend_closure_t *closure = (zend_closure_t*) source;
    zend_closure_t *copy =
        (zend_closure_t*)
            php_parallel_copy_mem(
                source, sizeof(zend_closure_t), 1);

    php_parallel_cache_closure(&closure->func, &copy->func);

    GC_ADD_FLAGS(&copy->std, GC_IMMUTABLE);

    copy->func.common.fn_flags |= ZEND_ACC_CLOSURE;

    if (Z_TYPE(copy->this_ptr) == IS_OBJECT) {
        PARALLEL_ZVAL_COPY(&copy->this_ptr, &copy->this_ptr, 1);
    }

    php_parallel_dependencies_store(&copy->func);

    return &copy->std;
}

static zend_always_inline void php_parallel_copy_closure_init_run_time_cache(zend_op_array *function) {
    void *rtc;

#ifdef ZEND_ACC_HEAP_RT_CACHE
    function->fn_flags |= ZEND_ACC_HEAP_RT_CACHE;
#else
    function->fn_flags |= ZEND_ACC_NO_RT_ARENA;
#endif

#ifdef ZEND_MAP_PTR_SET
    {
        rtc = emalloc(sizeof(void*) + function->cache_size);

        ZEND_MAP_PTR_INIT(function->run_time_cache, rtc);

        rtc = (char*)rtc + sizeof(void*);

        ZEND_MAP_PTR_SET(function->run_time_cache, rtc);
    }
#else
    function->run_time_cache = rtc = emalloc(function->cache_size);
#endif

    memset(rtc, 0, function->cache_size);
}

static zend_always_inline zend_object* php_parallel_copy_closure_thread(zend_object *source) {
    zend_closure_t *copy =
        (zend_closure_t*)
            php_parallel_copy_mem(
                source, sizeof(zend_closure_t), 0);
    zend_class_entry *scope =
        copy->func.op_array.scope;
    zend_op_array *function =
        (zend_op_array*) &copy->func;

    GC_DEL_FLAGS(&copy->std, GC_IMMUTABLE);

    zend_object_std_init(&copy->std, zend_ce_closure);

    if (copy->called_scope &&
        copy->called_scope->type == ZEND_USER_CLASS) {
        copy->called_scope =
            php_parallel_copy_scope(copy->called_scope);
    }

    if (scope &&
        scope->type == ZEND_USER_CLASS) {
        function->scope = php_parallel_copy_scope(scope);
    }

    if (function->static_variables) {
        function->static_variables =
            php_parallel_copy_hash_ctor(function->static_variables, 0);
    }

#ifdef ZEND_MAP_PTR_INIT
    ZEND_MAP_PTR_INIT(function->static_variables_ptr, &function->static_variables);
#endif

    php_parallel_copy_closure_init_run_time_cache(function);

#if PHP_VERSION_ID < 70300
    function->prototype = (void*) copy;
#endif

    if (Z_TYPE(copy->this_ptr) == IS_OBJECT) {
        PARALLEL_ZVAL_COPY(&copy->this_ptr, &copy->this_ptr, 0);
    }

    php_parallel_dependencies_load((zend_function*) function);

    return &copy->std;
}

static zend_always_inline zend_object* php_parallel_copy_closure_ctor(zend_object *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_closure_persistent(source);
    }

    return php_parallel_copy_closure_thread(source);
}

static zend_always_inline void php_parallel_copy_closure_dtor(zend_object *source, zend_bool persistent) {
    zend_closure_t *closure;

    if (!persistent) {
        OBJ_RELEASE(source);
        return;
    }

    closure = (zend_closure_t*) source;

    if (closure->func.op_array.static_variables) {
        php_parallel_copy_hash_dtor(
            closure->func.op_array.static_variables, 1);
    }

    if (Z_TYPE(closure->this_ptr) == IS_OBJECT) {
        PARALLEL_ZVAL_DTOR(&closure->this_ptr);
    }

    pefree(closure, 1);
}

static zend_always_inline zend_reference* php_parallel_copy_reference_persistent(zend_reference *source) {
    zend_reference *reference =
        php_parallel_copy_mem(source, sizeof(zend_reference), 1);

    GC_SET_REFCOUNT(reference, 1);
    GC_ADD_FLAGS(reference, GC_IMMUTABLE);

    PARALLEL_ZVAL_COPY(&reference->val, &source->val, 1);

    return reference;
}

static zend_always_inline zend_reference* php_parallel_copy_reference_thread(zend_reference *source) {
    zend_reference *reference =
        php_parallel_copy_mem(source, sizeof(zend_reference), 0);

    GC_DEL_FLAGS(reference, GC_IMMUTABLE);

    PARALLEL_ZVAL_COPY(&reference->val, &source->val, 0);

    return reference;
}

static zend_always_inline zend_reference* php_parallel_copy_reference_ctor(zend_reference *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_reference_persistent(source);
    }
    return php_parallel_copy_reference_thread(source);
}

static zend_always_inline void php_parallel_copy_reference_dtor(zend_reference *source, zend_bool persistent) {
    if (GC_DELREF(source) == 0) {
        PARALLEL_ZVAL_DTOR(
            &source->val);
        pefree(source, persistent);
    }
}

static size_t _php_parallel_copy_channel_size = 0;

static zend_always_inline size_t php_parallel_copy_channel_size() {
    if (_php_parallel_copy_channel_size == 0) {
        _php_parallel_copy_channel_size =
            sizeof(php_parallel_channel_t) +
            zend_object_properties_size(php_parallel_channel_ce);
    }

    return _php_parallel_copy_channel_size;
}

static zend_always_inline zend_object* php_parallel_copy_channel_persistent(zend_object *source) {
    php_parallel_channel_t *channel = php_parallel_channel_fetch(source),
                           *dest    = php_parallel_copy_mem(channel, php_parallel_copy_channel_size(), 1);

    GC_ADD_FLAGS(&dest->std, GC_IMMUTABLE);

    dest->link = php_parallel_link_copy(channel->link);

    return &dest->std;
}

static zend_always_inline zend_object* php_parallel_copy_channel_thread(zend_object *source) {
    php_parallel_channel_t *channel = php_parallel_channel_fetch(source),
                           *dest    = php_parallel_copy_mem(channel, php_parallel_copy_channel_size(), 0);

    GC_DEL_FLAGS(&dest->std, GC_IMMUTABLE);

    zend_object_std_init(&dest->std, php_parallel_channel_ce);

    dest->std.handlers = &php_parallel_channel_handlers;

    dest->link = php_parallel_link_copy(channel->link);

    return &dest->std;
}

static zend_always_inline zend_object* php_parallel_copy_channel_ctor(zend_object *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_channel_persistent(source);
    }
    return php_parallel_copy_channel_thread(source);
}

static zend_always_inline void php_parallel_copy_channel_dtor(zend_object *source, zend_bool persistent) {
    php_parallel_channel_t *channel = php_parallel_channel_fetch(source);

    if (!persistent) {
        OBJ_RELEASE(source);
        return;
    }

    php_parallel_link_destroy(channel->link);

    pefree(channel, persistent);
}

static size_t _php_parallel_copy_sync_size = 0;

static zend_always_inline size_t php_parallel_copy_sync_size() {
    if (_php_parallel_copy_sync_size == 0) {
        _php_parallel_copy_sync_size =
            sizeof(php_parallel_sync_object_t) +
            zend_object_properties_size(php_parallel_sync_ce);
    }

    return _php_parallel_copy_sync_size;
}

static zend_always_inline zend_object* php_parallel_copy_sync_persistent(zend_object *source) {
    php_parallel_sync_object_t *object  = php_parallel_sync_object_fetch(source),
                                 *dest    = php_parallel_copy_mem(object, php_parallel_copy_sync_size(), 1);

    GC_ADD_FLAGS(&dest->std, GC_IMMUTABLE);

    dest->sync = php_parallel_sync_copy(object->sync);

    return &dest->std;
}

static zend_always_inline zend_object* php_parallel_copy_sync_thread(zend_object *source) {
    php_parallel_sync_object_t *object = php_parallel_sync_object_fetch(source),
                                 *dest    = php_parallel_copy_mem(object, php_parallel_copy_sync_size(), 0);

    GC_DEL_FLAGS(&dest->std, GC_IMMUTABLE);

    zend_object_std_init(&dest->std, dest->std.ce);

    dest->sync = php_parallel_sync_copy(object->sync);

    return &dest->std;
}

static zend_always_inline zend_object* php_parallel_copy_sync_ctor(zend_object *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_sync_persistent(source);
    }
    return php_parallel_copy_sync_thread(source);
}

static zend_always_inline void php_parallel_copy_sync_dtor(zend_object *source, zend_bool persistent) {
    php_parallel_sync_object_t *object = php_parallel_sync_object_fetch(source);

    if (!persistent) {
        OBJ_RELEASE(source);
        return;
    }

    php_parallel_sync_release(object->sync);

    pefree(object, persistent);
}

static zend_always_inline zend_object* php_parallel_copy_object_persistent(zend_object *source) {
    zend_object *dest;
    zend_class_entry *ce;

    if (source->ce->create_object) {
        ce = php_parallel_copy_object_unavailable_ce;
    } else {
        ce = source->ce;
    }

    dest = php_parallel_copy_mem(
            source,
            sizeof(zend_object) + zend_object_properties_size(ce), 1);

    GC_SET_REFCOUNT(dest, 1);
    GC_ADD_FLAGS(dest, GC_IMMUTABLE);

    if (ce == php_parallel_copy_object_unavailable_ce) {
        dest->ce = ce;
        dest->handlers = zend_get_std_object_handlers();
    }

    if (ce->default_properties_count) {
        zval *property   = source->properties_table,
             *slot       = dest->properties_table,
             *end        = property + source->ce->default_properties_count;

        while (property < end) {
            PARALLEL_ZVAL_COPY(slot, property, 1);
            property++;
            slot++;
        }
    }

    if (dest->properties) {
        dest->properties =
            php_parallel_copy_hash_ctor(source->properties, 1);
    }

    return dest;
}

static zend_always_inline zend_object* php_parallel_copy_object_thread(zend_object *source) {
    zend_object *dest;
    zend_class_entry *ce;

    if (source->ce->create_object) {
        ce = php_parallel_copy_object_unavailable_ce;
    } else {
        ce = php_parallel_copy_scope(source->ce);
    }

    dest = php_parallel_copy_mem(
            source,
            sizeof(zend_object) + zend_object_properties_size(ce), 0);

    if (ce == php_parallel_copy_object_unavailable_ce) {
        dest->ce = ce;
        dest->handlers = zend_get_std_object_handlers();
    }

    zend_object_std_init(dest, ce);

    GC_DEL_FLAGS(dest, GC_IMMUTABLE);

    if (ce->default_properties_count) {
        zval *property   = source->properties_table,
             *slot       = dest->properties_table,
             *end        = property + source->ce->default_properties_count;

        while (property < end) {
            PARALLEL_ZVAL_COPY(slot, property, 0);
            property++;
            slot++;
        }
    }

    if (source->properties) {
        dest->properties =
            php_parallel_copy_hash_ctor(source->properties, 0);
    }

    return dest;
}

static zend_always_inline zend_object* php_parallel_copy_object_ctor(zend_object *source, zend_bool persistent) {
    if (source->ce == zend_ce_closure) {
        return php_parallel_copy_closure_ctor(source, persistent);
    }

    if (source->ce == php_parallel_channel_ce) {
        return php_parallel_copy_channel_ctor(source, persistent);
    }

    if (instanceof_function(source->ce, php_parallel_sync_ce)) {
        return php_parallel_copy_sync_ctor(source, persistent);
    }

    if (persistent) {
        return php_parallel_copy_object_persistent(source);
    }

    return php_parallel_copy_object_thread(source);
}

static zend_always_inline void php_parallel_copy_object_dtor(zend_object *source, zend_bool persistent) {
    if (source->ce == zend_ce_closure) {
        php_parallel_copy_closure_dtor(source, persistent);
        return;
    }

    if (source->ce == php_parallel_channel_ce) {
        php_parallel_copy_channel_dtor(source, persistent);
        return;
    }

    if (instanceof_function(source->ce, php_parallel_sync_ce)) {
        php_parallel_copy_sync_dtor(source, persistent);
        return;
    }

    if (!persistent) {
        OBJ_RELEASE(source);
        return;
    }

    if (source->ce->default_properties_count) {
        zval *property = source->properties_table,
             *end      = property + source->ce->default_properties_count;

        while (property < end) {
            PARALLEL_ZVAL_DTOR(property);
            property++;
        }
    }

    if (source->properties) {
        php_parallel_copy_hash_dtor(source->properties, 1);
    }

    pefree(source, 1);
}

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
            ZVAL_STR(dest, php_parallel_copy_string_ctor(Z_STR_P(source), persistent));
        break;

        case IS_ARRAY:
            ZVAL_ARR(dest, php_parallel_copy_hash_ctor(Z_ARRVAL_P(source), persistent));
        break;

        case IS_OBJECT: {
            zend_object *copy = php_parallel_copy_object_ctor(Z_OBJ_P(source), persistent);

            if (!copy) {
                ZVAL_NULL(dest);
            } else {
                ZVAL_OBJ(dest, copy);
            }
        } break;

        case IS_REFERENCE:
            ZVAL_REF(dest, php_parallel_copy_reference_ctor(Z_REF_P(source), persistent));
        break;

        case IS_RESOURCE: {
            zend_long fd = php_parallel_copy_resource_ctor(Z_RES_P(source), persistent);

            if (fd == -1) {
                ZVAL_NULL(dest);
            } else {
                ZVAL_LONG(dest, fd);
            }
        } break;

        default:
            ZVAL_BOOL(dest, zend_is_true(source));
    }
}

void php_parallel_copy_zval_dtor(zval *zv) {
    switch (Z_TYPE_P(zv)) {
        case IS_ARRAY:
            php_parallel_copy_hash_dtor(Z_ARRVAL_P(zv), GC_FLAGS(Z_ARRVAL_P(zv)) & IS_ARRAY_IMMUTABLE);
        break;

        case IS_STRING:
            php_parallel_copy_string_dtor(Z_STR_P(zv), GC_FLAGS(Z_STR_P(zv)) & IS_STR_PERSISTENT);
        break;

        case IS_REFERENCE:
            php_parallel_copy_reference_dtor(Z_REF_P(zv), GC_FLAGS(Z_REF_P(zv)) & GC_IMMUTABLE);
        break;

        case IS_OBJECT:
            php_parallel_copy_object_dtor(Z_OBJ_P(zv), GC_FLAGS(Z_OBJ_P(zv)) & GC_IMMUTABLE);
        break;

        default:
            zval_ptr_dtor(zv);
    }
}

zend_function* php_parallel_copy_function(const zend_function *function, zend_bool persistent) {
    if (persistent) {
        function =      	
            php_parallel_cache_function(function);

        php_parallel_dependencies_store(function);
    } else {
        php_parallel_dependencies_load(function);
    }

    return (zend_function*) function;
}

PHP_RINIT_FUNCTION(PARALLEL_COPY)
{
    zend_hash_init(&PCG(scope),     32, NULL, NULL, 0);

    PHP_RINIT(PARALLEL_CHECK)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_RINIT(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);

    object_init_ex(&PCG(unavailable), php_parallel_copy_object_unavailable_ce);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_COPY)
{
    zend_hash_destroy(&PCG(scope));

    PHP_RSHUTDOWN(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_RSHUTDOWN(PARALLEL_CHECK)(INIT_FUNC_ARGS_PASSTHRU);

    zval_ptr_dtor(&PCG(unavailable));

    return SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_COPY_STRINGS)
{
    zend_hash_init(&PCS(table), 32, NULL, php_parallel_copy_string_free, 1);

    return SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_COPY)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Type", "Unavailable", NULL);

    php_parallel_copy_type_unavailable_ce = zend_register_internal_class(&ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Object", "Unavailable", NULL);

    php_parallel_copy_object_unavailable_ce = zend_register_internal_class(&ce);

    PHP_MINIT(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_CACHE)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_COPY_STRINGS)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(PARALLEL_COPY_STRINGS)
{
    zend_hash_destroy(&PCS(table));

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_COPY)
{
    PHP_MSHUTDOWN(PARALLEL_CACHE)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_DEPENDENCIES)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_COPY_STRINGS)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}
#endif
