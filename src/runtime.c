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
#ifndef HAVE_PARALLEL_RUNTIME
#define HAVE_PARALLEL_RUNTIME

#include "parallel.h"

zend_class_entry *php_parallel_runtime_ce;
zend_object_handlers php_parallel_runtime_handlers;

static void php_parallel_runtime_functions_dtor(zval *zv) {
    php_parallel_copy_function_free(Z_FUNC_P(zv), 0);
}

void php_parallel_runtime_functions_setup(php_parallel_runtime_functions_t *functions, zend_bool thread) {
    if (thread) {
        zend_hash_init(&functions->lambdas,   16, NULL, php_parallel_runtime_functions_dtor, 0);
        zend_hash_init(&functions->functions, 16, NULL, php_parallel_runtime_functions_dtor, 0);
    } else {
        zend_hash_init(&functions->lambdas,   16, NULL, NULL, 1);
        zend_hash_init(&functions->functions, 16, NULL, NULL, 1);
    }
}

void php_parallel_runtime_function_push(php_parallel_runtime_t *runtime, zend_string *name, const zend_function *function, zend_bool lambda) {
    php_parallel_monitor_lock(runtime->monitor);

    zend_hash_add_ptr(
        lambda ?
            &runtime->functions.lambdas :
            &runtime->functions.functions,
        name,
        php_parallel_copy_function(function, 1));

    php_parallel_monitor_unlock(runtime->monitor);
}

static zend_always_inline void php_parallel_runtime_functions_destroy(php_parallel_runtime_functions_t *functions) {
    zend_hash_destroy(&functions->lambdas);
    zend_hash_destroy(&functions->functions);
}

void php_parallel_runtime_functions_finish(php_parallel_runtime_functions_t *functions) {
    dtor_func_t dtor = EG(function_table)->pDestructor;
    zend_string *key, *rtd;
    zend_function *function;

    EG(function_table)->pDestructor = NULL;
    ZEND_HASH_FOREACH_STR_KEY(&functions->lambdas, key) {
        zend_hash_del(EG(function_table), key);
    } ZEND_HASH_FOREACH_END();
    ZEND_HASH_FOREACH_STR_KEY_PTR(&functions->functions, rtd, function) {
        zend_string *key =
            zend_string_tolower(function->common.function_name);
        zend_hash_del(EG(function_table), key);
        zend_hash_del(EG(function_table), rtd);
        zend_string_release(key);
    } ZEND_HASH_FOREACH_END();
    EG(function_table)->pDestructor = dtor;

    php_parallel_runtime_functions_destroy(functions);
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_runtime_construct_arginfo, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, bootstrap, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Runtime, __construct)
{
    php_parallel_runtime_t *runtime = php_parallel_runtime_from(getThis());
    zend_string            *bootstrap = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(bootstrap)
    ZEND_PARSE_PARAMETERS_END();

    php_parallel_scheduler_start(runtime, bootstrap);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(php_parallel_runtime_run_arginfo, 0, 1, \\parallel\\Future, 1)
    ZEND_ARG_OBJ_INFO(0, task, Closure, 0)
    ZEND_ARG_TYPE_INFO(0, argv, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Runtime, run)
{
    php_parallel_runtime_t  *runtime = php_parallel_runtime_from(getThis());
    zval *closure = NULL;
    zval *argv = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT_OF_CLASS(closure, zend_ce_closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();

    if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_closed_ce,
            "Runtime closed");
        return;
    }

    php_parallel_scheduler_push(runtime, closure, argv, return_value);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_runtime_close_or_kill_arginfo, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Runtime, close)
{
    php_parallel_runtime_t *runtime =
        php_parallel_runtime_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_closed_ce,
            "Runtime closed");
        return;
    }

    php_parallel_scheduler_join(runtime, 0);
}

PHP_METHOD(Runtime, kill)
{
    php_parallel_runtime_t *runtime =
        php_parallel_runtime_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    if (php_parallel_monitor_check(runtime->monitor, PHP_PARALLEL_CLOSED)) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_closed_ce,
            "Runtime closed");
        return;
    }

    php_parallel_scheduler_join(runtime, 1);
}

zend_function_entry php_parallel_runtime_methods[] = {
    PHP_ME(Runtime, __construct, php_parallel_runtime_construct_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Runtime, run, php_parallel_runtime_run_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Runtime, close, php_parallel_runtime_close_or_kill_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Runtime, kill, php_parallel_runtime_close_or_kill_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zend_object* php_parallel_runtime_create(zend_class_entry *type) {
    php_parallel_runtime_t *runtime = ecalloc(1,
            sizeof(php_parallel_runtime_t) + zend_object_properties_size(type));

    zend_object_std_init(&runtime->std, type);

    runtime->std.handlers = &php_parallel_runtime_handlers;

    php_parallel_scheduler_init(runtime);

    runtime->parent.server = SG(server_context);

    php_parallel_runtime_functions_setup(&runtime->functions, 0);

    return &runtime->std;
}

void php_parallel_runtime_destroy(zend_object *o) {
    php_parallel_runtime_t *runtime =
        php_parallel_runtime_fetch(o);

    php_parallel_scheduler_stop(runtime);

    php_parallel_scheduler_destroy(runtime);

    php_parallel_runtime_functions_destroy(&runtime->functions);

    zend_object_std_dtor(o);
}

void php_parallel_runtime_startup() {
    zend_class_entry ce;

    memcpy(&php_parallel_runtime_handlers, php_parallel_standard_handlers(), sizeof(zend_object_handlers));

    php_parallel_runtime_handlers.offset = XtOffsetOf(php_parallel_runtime_t, std);
    php_parallel_runtime_handlers.free_obj = php_parallel_runtime_destroy;

    INIT_NS_CLASS_ENTRY(ce, "parallel", "Runtime", php_parallel_runtime_methods);

    php_parallel_runtime_ce = zend_register_internal_class(&ce);
    php_parallel_runtime_ce->create_object = php_parallel_runtime_create;
    php_parallel_runtime_ce->ce_flags |= ZEND_ACC_FINAL;
}

void php_parallel_runtime_shutdown() {

}
#endif
