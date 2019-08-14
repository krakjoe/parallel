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
#ifndef HAVE_PARALLEL_FUTURE
#define HAVE_PARALLEL_FUTURE

#include "parallel.h"

zend_class_entry *php_parallel_future_ce;
zend_object_handlers php_parallel_future_handlers;

zend_string *php_parallel_future_string_runtime;

zend_bool php_parallel_future_lock(php_parallel_future_t *future) {
    return php_parallel_monitor_lock(future->monitor);
}

zend_bool php_parallel_future_readable(php_parallel_future_t *future) {
    return php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY);
}

static zend_always_inline void php_parallel_future_value_inline(php_parallel_future_t *future, zval *return_value) {
    if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_DONE)) {
        zval garbage = future->value;

        PARALLEL_ZVAL_COPY(
            &future->value, &garbage, 0);

        if (Z_OPT_REFCOUNTED(garbage)) {
            PARALLEL_ZVAL_DTOR(&garbage);
        }

        php_parallel_monitor_set(future->monitor, PHP_PARALLEL_DONE);
    }

    ZVAL_COPY(return_value, &future->value);
}

void php_parallel_future_value(php_parallel_future_t *future, zval *return_value) {
    php_parallel_monitor_lock(future->monitor);

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_ERROR)) {
        ZVAL_OBJ(return_value,
            php_parallel_exceptions_restore(&future->value));
        php_parallel_monitor_unlock(future->monitor);
        return;
    } else if (php_parallel_monitor_check(future->monitor,
                PHP_PARALLEL_KILLED|PHP_PARALLEL_CANCELLED)) {
        ZVAL_NULL(return_value);
        php_parallel_monitor_unlock(future->monitor);
        return;
    }

    php_parallel_future_value_inline(future, return_value);

    php_parallel_monitor_unlock(future->monitor);
}

zend_bool php_parallel_future_unlock(php_parallel_future_t *future) {
    return php_parallel_monitor_unlock(future->monitor);
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_future_value_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Future, value)
{
    php_parallel_future_t *future = php_parallel_future_from(getThis());
    int32_t state;

    PARALLEL_PARAMETERS_NONE(return);

    php_parallel_monitor_lock(future->monitor);

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_CANCELLED)) {
        php_parallel_exception_ex(
            php_parallel_future_error_cancelled_ce,
            "cannot retrieve value");
        php_parallel_monitor_unlock(future->monitor);
        return;
    }

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_KILLED)) {
        php_parallel_exception_ex(
            php_parallel_future_error_killed_ce,
            "cannot retrieve value");
        php_parallel_monitor_unlock(future->monitor);
        return;
    }

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_DONE)) {
        php_parallel_monitor_unlock(future->monitor);

        goto _php_parallel_future_value;
    } else {
        state = php_parallel_monitor_wait_locked(future->monitor,
                    PHP_PARALLEL_READY|
                    PHP_PARALLEL_FAILURE|
                    PHP_PARALLEL_ERROR);
        php_parallel_monitor_unlock(future->monitor);
    }

    if ((state == FAILURE) || (state & PHP_PARALLEL_FAILURE)) {
        php_parallel_exception_ex(
            php_parallel_future_error_ce,
            "cannot retrieve value");
        php_parallel_monitor_set(future->monitor,
            PHP_PARALLEL_READY|PHP_PARALLEL_FAILURE);
        return;
    }

    if ((state & PHP_PARALLEL_ERROR)) {
        zval exception;

        ZVAL_OBJ(&exception,
            php_parallel_exceptions_restore(&future->value));

        php_parallel_monitor_set(future->monitor,
            PHP_PARALLEL_READY|PHP_PARALLEL_ERROR);

        zend_throw_exception_object(&exception);
        return;
    }

    php_parallel_monitor_set(future->monitor, PHP_PARALLEL_READY);

_php_parallel_future_value:
    php_parallel_future_value_inline(future, return_value);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_future_cancel_arginfo, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Future, cancel)
{
    php_parallel_future_t *future =
        php_parallel_future_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_CANCELLED)) {
        php_parallel_exception_ex(
            php_parallel_future_error_cancelled_ce,
            "task was already cancelled");
        return;
    }

    if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_KILLED)) {
        php_parallel_exception_ex(
            php_parallel_future_error_killed_ce,
            "runtime executing task was killed");
        return;
    }

    RETURN_BOOL(php_parallel_scheduler_cancel(future));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_future_cancelled_arginfo, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Future, cancelled)
{
    php_parallel_future_t *future =
        php_parallel_future_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    RETURN_BOOL(php_parallel_monitor_check(future->monitor, PHP_PARALLEL_CANCELLED));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_future_done_arginfo, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Future, done)
{
    php_parallel_future_t *future =
        php_parallel_future_from(getThis());

    RETURN_BOOL(php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY));
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_future_construct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Future, __construct)
{
    php_parallel_future_t *future = php_parallel_future_from(getThis());

    php_parallel_exception_ex(
        php_parallel_future_error_ce,
        "construction of Future objects is not allowed");

    php_parallel_monitor_set(future->monitor, PHP_PARALLEL_READY|PHP_PARALLEL_DONE);
}

zend_function_entry php_parallel_future_methods[] = {
    PHP_ME(Future, __construct, php_parallel_future_construct_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Future, value, php_parallel_future_value_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Future, done, php_parallel_future_done_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Future, cancel, php_parallel_future_cancel_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Future, cancelled, php_parallel_future_cancelled_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_object* php_parallel_future_create(zend_class_entry *type) {
    php_parallel_future_t *future = ecalloc(1,
            sizeof(php_parallel_future_t) + zend_object_properties_size(type));

    zend_object_std_init(&future->std, type);

    future->std.handlers = &php_parallel_future_handlers;

    future->monitor = php_parallel_monitor_create();

    return &future->std;
}

void php_parallel_future_destroy(zend_object *o) {
    php_parallel_future_t *future =
        php_parallel_future_fetch(o);

    php_parallel_monitor_lock(future->monitor);

    if (!php_parallel_monitor_check(future->monitor, PHP_PARALLEL_READY)) {
        php_parallel_monitor_wait_locked(future->monitor, PHP_PARALLEL_READY);
    }

    php_parallel_monitor_unlock(future->monitor);

    switch (Z_TYPE(future->value)) {
        case IS_PTR:
            php_parallel_exceptions_destroy(Z_PTR(future->value));
        break;

        default:
            if (Z_OPT_REFCOUNTED(future->value)) {
                PARALLEL_ZVAL_DTOR(&future->value);
            }
    }

    if (future->runtime) {
       OBJ_RELEASE(&future->runtime->std);
    }

    php_parallel_monitor_destroy(future->monitor);

    zend_object_std_dtor(o);
}

#if PHP_VERSION_ID >= 80000
static HashTable* php_parallel_future_debug(zend_object *zo, int *temp) {
    php_parallel_future_t *future = php_parallel_future_fetch(zo);
#else
static HashTable* php_parallel_future_debug(zval *zv, int *temp) {
    php_parallel_future_t *future = php_parallel_future_from(zv);
#endif
    HashTable *debug;
    zval zdbg;

    ALLOC_HASHTABLE(debug);
    zend_hash_init(debug, 3, NULL, ZVAL_PTR_DTOR, 0);

    *temp = 1;

    GC_ADDREF(&future->runtime->std);

    ZVAL_OBJ(&zdbg, &future->runtime->std);

    zend_hash_add(
        debug,
        php_parallel_future_string_runtime,
        &zdbg);

    return debug;
}

PHP_MINIT_FUNCTION(PARALLEL_FUTURE)
{
    zend_class_entry ce;

    memcpy(&php_parallel_future_handlers, php_parallel_standard_handlers(), sizeof(zend_object_handlers));

    php_parallel_future_handlers.offset = XtOffsetOf(php_parallel_future_t, std);
    php_parallel_future_handlers.free_obj = php_parallel_future_destroy;
    php_parallel_future_handlers.get_debug_info = php_parallel_future_debug;

    INIT_NS_CLASS_ENTRY(ce, "parallel", "Future", php_parallel_future_methods);

    php_parallel_future_ce = zend_register_internal_class(&ce);
    php_parallel_future_ce->create_object = php_parallel_future_create;
    php_parallel_future_ce->ce_flags |= ZEND_ACC_FINAL;

    php_parallel_future_ce->serialize = zend_class_serialize_deny;
    php_parallel_future_ce->unserialize = zend_class_unserialize_deny;

    php_parallel_future_string_runtime = zend_string_init_interned(ZEND_STRL("runtime"), 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_FUTURE)
{
    zend_string_release(php_parallel_future_string_runtime);

    return SUCCESS;
}
#endif
