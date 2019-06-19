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
#ifndef HAVE_PARALLEL_SYNC
#define HAVE_PARALLEL_SYNC

#include "parallel.h"

zend_class_entry *php_parallel_sync_ce;
zend_object_handlers php_parallel_sync_handlers;

static zend_string *php_parallel_sync_string_value;

#define PARALLEL_SYNC_IS_SCALAR(zv) \
    ((Z_TYPE_P(zv) != IS_OBJECT) && \
     (Z_TYPE_P(zv) != IS_ARRAY) && \
     (Z_TYPE_P(zv) != IS_RESOURCE))

static zend_always_inline php_parallel_sync_t* php_parallel_sync_fetch(zend_object *o) {
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_fetch(o);

    return object->sync;
}

static zend_always_inline php_parallel_sync_t* php_parallel_sync_from(zval *z) {
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_from(z);

    return object->sync;
}

static zend_always_inline php_parallel_sync_t* php_parallel_sync_create(zval *zv) {
    php_parallel_sync_t *sync =
        (php_parallel_sync_t*) pecalloc(1, sizeof(php_parallel_sync_t), 1);

    php_parallel_mutex_init(&sync->mutex, 1);
    php_parallel_cond_init(&sync->condition);

    if (zv) {
        PARALLEL_ZVAL_COPY(
            &sync->value, zv, 1);
    }

    sync->refcount = 1;

    return sync;
}

php_parallel_sync_t* php_parallel_sync_copy(php_parallel_sync_t *sync) {
    php_parallel_atomic_addref(&sync->refcount);

    return sync;
}

void php_parallel_sync_release(php_parallel_sync_t *sync) {
    if (!sync) {
        return;
    }

    if (php_parallel_atomic_delref(&sync->refcount) == 0) {
        if (Z_TYPE(sync->value) == IS_STRING) {
            PARALLEL_ZVAL_DTOR(&sync->value);
        }

        php_parallel_mutex_destroy(&sync->mutex);
        php_parallel_cond_destroy(&sync->condition);
        pefree(sync, 1);
    }
}

static zend_object* php_parallel_sync_object_create(zend_class_entry *ce) {
    php_parallel_sync_object_t *object =
        (php_parallel_sync_object_t*)
            ecalloc(1, sizeof(php_parallel_sync_t) + zend_object_properties_size(ce));

    zend_object_std_init(&object->std, ce);

    object->std.handlers = &php_parallel_sync_handlers;

    return &object->std;
}

static void php_parallel_sync_object_destroy(zend_object *o) {
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_fetch(o);

    php_parallel_sync_release(object->sync);

    zend_object_std_dtor(o);
}

#if PHP_VERSION_ID < 80000
static HashTable* php_parallel_sync_object_debug(zval *zo, int *temp) {
    zend_object *o = Z_OBJ_P(zo);
#else
static HashTable* php_parallel_sync_object_debug(zend_object *o, int *temp) {
#endif
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_fetch(o);
    HashTable *debug;
    zval zv;

    *temp = 1;

    pthread_mutex_lock(&object->sync->mutex);

    ALLOC_HASHTABLE(debug);
    zend_hash_init(debug, 1, NULL, ZVAL_PTR_DTOR, 0);

    if (!Z_ISUNDEF(object->sync->value)) {
        PARALLEL_ZVAL_COPY(
            &zv, &object->sync->value, 0);

        zend_hash_add(debug, php_parallel_sync_string_value, &zv);
    }

    pthread_mutex_unlock(&object->sync->mutex);

    return debug;
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_sync_construct_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, __construct)
{
    php_parallel_sync_object_t *object = php_parallel_sync_object_from(getThis());
    zval *value = NULL;

    if (ZEND_NUM_ARGS()) {
      ZEND_PARSE_PARAMETERS_START(0, 1)
          Z_PARAM_OPTIONAL
          Z_PARAM_ZVAL(value)
      ZEND_PARSE_PARAMETERS_END();
    }

    if (value) {
        if (!PARALLEL_SYNC_IS_SCALAR(value)) {
            php_parallel_exception_ex(
                php_parallel_sync_error_illegal_value_ce,
                "sync cannot contain non-scalar %s",
                Z_TYPE_P(value) == IS_OBJECT ?
                    ZSTR_VAL(Z_OBJCE_P(value)->name) :
                    zend_get_type_by_const(Z_TYPE_P(value)));
            return;
        }
    }

    object->sync = php_parallel_sync_create(value);
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_sync_set_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, set)
{
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_from(getThis());
    zval *value;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    if (!PARALLEL_SYNC_IS_SCALAR(value)) {
        php_parallel_exception_ex(
            php_parallel_sync_error_illegal_value_ce,
            "sync cannot contain non-scalar %s",
            Z_TYPE_P(value) == IS_OBJECT ?
                ZSTR_VAL(Z_OBJCE_P(value)->name) :
                zend_get_type_by_const(Z_TYPE_P(value)));
        return;
    }

    pthread_mutex_lock(&object->sync->mutex);

    if (Z_TYPE(object->sync->value) == IS_STRING) {
        PARALLEL_ZVAL_DTOR(&object->sync->value);
    }

    PARALLEL_ZVAL_COPY(
        &object->sync->value, value, 1);

    pthread_mutex_unlock(&object->sync->mutex);
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_sync_get_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, get)
{
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_from(getThis());

    pthread_mutex_lock(&object->sync->mutex);

    if (Z_TYPE(object->sync->value) != IS_UNDEF) {
        PARALLEL_ZVAL_COPY(
          return_value, &object->sync->value, 0);
    }

    pthread_mutex_unlock(&object->sync->mutex);
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_sync_invoke_arginfo, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, block, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, __invoke)
{
    php_parallel_sync_object_t *object = php_parallel_sync_object_from(getThis());
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_FUNC(fci, fcc)
    ZEND_PARSE_PARAMETERS_END();

    pthread_mutex_lock(&object->sync->mutex);

    fci.retval = return_value;

    zend_try {
        zend_call_function(&fci, &fcc);
    } zend_end_try();

    pthread_mutex_unlock(&object->sync->mutex);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_sync_wait_arginfo, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, wait)
{
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_from(getThis());

    switch (pthread_cond_wait(&object->sync->condition, &object->sync->mutex)) {
        case SUCCESS:
            RETURN_TRUE;

        /* LCOV_EXCL_START */
        default:
            RETURN_FALSE;
        /* LCOV_EXCL_STOP */
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_sync_notify_arginfo, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, all, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Sync, notify)
{
    php_parallel_sync_object_t *object =
        php_parallel_sync_object_from(getThis());
    zend_bool all = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(all)
    ZEND_PARSE_PARAMETERS_END();

    switch (all ?
        pthread_cond_broadcast(&object->sync->condition) :
        pthread_cond_signal(&object->sync->condition)) {
        case SUCCESS:
            RETURN_TRUE;

        /* LCOV_EXCL_START */
        default:
            RETURN_FALSE;
        /* LCOV_EXCL_STOP */
    }
}

static zend_function_entry php_parallel_sync_methods[] = {
    PHP_ME(Sync, __construct, php_parallel_sync_construct_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Sync, set, php_parallel_sync_set_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Sync, get, php_parallel_sync_get_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Sync, wait, php_parallel_sync_wait_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Sync, notify, php_parallel_sync_notify_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Sync, __invoke,    php_parallel_sync_invoke_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PARALLEL_SYNC)
{
    zend_class_entry ce;

    memcpy(&php_parallel_sync_handlers, php_parallel_standard_handlers(), sizeof(zend_object_handlers));

    php_parallel_sync_handlers.offset = XtOffsetOf(php_parallel_sync_object_t, std);
    php_parallel_sync_handlers.free_obj = php_parallel_sync_object_destroy;
    php_parallel_sync_handlers.get_debug_info = php_parallel_sync_object_debug;

    INIT_NS_CLASS_ENTRY(ce, "parallel", "Sync", php_parallel_sync_methods);

    php_parallel_sync_ce = zend_register_internal_class(&ce);
    php_parallel_sync_ce->create_object = php_parallel_sync_object_create;
    php_parallel_sync_ce->ce_flags |= ZEND_ACC_FINAL;

    php_parallel_sync_ce->serialize = zend_class_serialize_deny;
    php_parallel_sync_ce->unserialize = zend_class_unserialize_deny;

    php_parallel_sync_string_value = zend_string_init_interned(ZEND_STRL("value"), 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_SYNC)
{
    return SUCCESS;
}
#endif
