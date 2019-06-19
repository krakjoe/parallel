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
#ifndef HAVE_PARALLEL_EVENTS_INPUT
#define HAVE_PARALLEL_EVENTS_INPUT

#include "parallel.h"

zend_class_entry* php_parallel_events_input_ce;
zend_object_handlers php_parallel_events_input_handlers;

typedef struct _php_parallel_events_input_t {
    HashTable   table;
    zend_object std;
} php_parallel_events_input_t;

static zend_always_inline php_parallel_events_input_t* php_parallel_events_input_fetch(zend_object *o) {
    return (php_parallel_events_input_t*) (((char*) o) - XtOffsetOf(php_parallel_events_input_t, std));
}

static zend_always_inline php_parallel_events_input_t* php_parallel_events_input_from(zval *z) {
    return php_parallel_events_input_fetch(Z_OBJ_P(z));
}

static zend_object* php_parallel_events_input_create(zend_class_entry *type) {
    php_parallel_events_input_t *input =
        (php_parallel_events_input_t*)
            ecalloc(1, sizeof(php_parallel_events_input_t) + zend_object_properties_size(type));

    zend_object_std_init(&input->std, type);

    input->std.handlers = &php_parallel_events_input_handlers;

    zend_hash_init(&input->table, 32, NULL, ZVAL_PTR_DTOR, 0);

    return &input->std;
}

static void php_parallel_events_input_destroy(zend_object *zo) {
    php_parallel_events_input_t *input =
        php_parallel_events_input_fetch(zo);

    zend_hash_destroy(&input->table);

    zend_object_std_dtor(zo);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_input_add_arginfo, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, target, IS_STRING, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Input, add)
{
    php_parallel_events_input_t *input =
        php_parallel_events_input_from(getThis());
    zend_string *target;
    zval        *value;
    zval        *error;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(target)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    if (!PARALLEL_ZVAL_CHECK(value, &error)) {
        php_parallel_exception_ex(
            php_parallel_events_input_error_illegal_value_ce,
            "value of type %s is illegal",
            Z_TYPE_P(error) == IS_OBJECT ?
                ZSTR_VAL(Z_OBJCE_P(error)->name) :
                zend_get_type_by_const(Z_TYPE_P(error)));
        return;
    }

    target = php_parallel_copy_string_interned(target);

    if (!zend_hash_add(&input->table, target, value)) {
        php_parallel_exception_ex(
            php_parallel_events_input_error_existence_ce,
            "input for target %s exists", ZSTR_VAL(target));
        return;
    }

    Z_TRY_ADDREF_P(value);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_input_remove_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, target, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Input, remove)
{
    php_parallel_events_input_t *input =
        php_parallel_events_input_from(getThis());
    zend_string *target;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(target)
    ZEND_PARSE_PARAMETERS_END();

    if (zend_hash_del(&input->table, target) != SUCCESS) {
        php_parallel_exception_ex(
            php_parallel_events_input_error_existence_ce,
            "input for target %s does not exist", ZSTR_VAL(target));
        return;
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_input_clear_arginfo, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Input, clear)
{
    php_parallel_events_input_t *input =
        php_parallel_events_input_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    zend_hash_clean(&input->table);
}

zend_function_entry php_parallel_events_input_methods[] = {
    PHP_ME(Input, add, php_parallel_events_input_add_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Input, remove, php_parallel_events_input_remove_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Input, clear, php_parallel_events_input_clear_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zval* php_parallel_events_input_find(zval *zv, zend_string *target) {
    php_parallel_events_input_t *input;

    if (Z_TYPE_P(zv) != IS_OBJECT) {
        return NULL;
    }

    input = php_parallel_events_input_from(zv);

    return zend_hash_find(&input->table, target);
}

zend_bool php_parallel_events_input_exists(zval *zv, zend_string *target) {
    php_parallel_events_input_t *input;

    if (Z_TYPE_P(zv) != IS_OBJECT) {
        return 0;
    }

    input = php_parallel_events_input_from(zv);

    return zend_hash_exists(&input->table, target);
}

zend_bool php_parallel_events_input_remove(zval *zv, zend_string *target) {
    php_parallel_events_input_t *input;

    if (Z_TYPE_P(zv) != IS_OBJECT) {
        return 0;
    }

    input = php_parallel_events_input_from(zv);

    return zend_hash_del(&input->table, target) == SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_EVENTS_INPUT)
{
    zend_class_entry ce;

    memcpy(
        &php_parallel_events_input_handlers,
        php_parallel_standard_handlers(),
        sizeof(zend_object_handlers));

    php_parallel_events_input_handlers.offset = XtOffsetOf(php_parallel_events_input_t, std);
    php_parallel_events_input_handlers.free_obj = php_parallel_events_input_destroy;

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Input", php_parallel_events_input_methods);

    php_parallel_events_input_ce = zend_register_internal_class(&ce);
    php_parallel_events_input_ce->create_object = php_parallel_events_input_create;
    php_parallel_events_input_ce->ce_flags |= ZEND_ACC_FINAL;

    php_parallel_events_input_ce->serialize = zend_class_serialize_deny;
    php_parallel_events_input_ce->unserialize = zend_class_unserialize_deny;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS_INPUT)
{
    return SUCCESS;
}
#endif
