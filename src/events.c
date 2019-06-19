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
#ifndef HAVE_PARALLEL_EVENTS
#define HAVE_PARALLEL_EVENTS

#include "parallel.h"
#include "poll.h"
#include "loop.h"

zend_class_entry* php_parallel_events_ce;
zend_object_handlers php_parallel_events_handlers;

static zend_object* php_parallel_events_create(zend_class_entry *type) {
    php_parallel_events_t *events =
        (php_parallel_events_t*) ecalloc(1,
            sizeof(php_parallel_events_t) + zend_object_properties_size(type));

    zend_object_std_init(&events->std, type);

    events->std.handlers = &php_parallel_events_handlers;

    zend_hash_init(
        &events->targets, 32, NULL, ZVAL_PTR_DTOR, 0);

    events->timeout = -1;
    events->blocking = 1;

    ZVAL_UNDEF(&events->input);

    return &events->std;
}

static zend_always_inline zend_bool php_parallel_events_add(php_parallel_events_t *events, zend_string *name, zval *object, zend_string **key) {
    if(instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
        php_parallel_channel_t *channel =
            (php_parallel_channel_t*)
                 php_parallel_channel_from(object);

        name = php_parallel_link_name(channel->link);
    } else {
        name = php_parallel_copy_string_interned(name);
    }

    *key = name;

    if (!zend_hash_add(&events->targets, name, object)) {
        return 0;
    }

    Z_ADDREF_P(object);

    return 1;
}

static zend_always_inline zend_bool php_parallel_events_remove(php_parallel_events_t *events, zend_string *name) {
    return zend_hash_del(&events->targets, name) == SUCCESS;
}

static void php_parallel_events_destroy(zend_object *zo) {
    php_parallel_events_t *events =
        php_parallel_events_fetch(zo);

    if (!Z_ISUNDEF(events->input)) {
        zval_ptr_dtor(&events->input);
    }

    zend_hash_destroy(&events->targets);

    zend_object_std_dtor(zo);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_set_input_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, input, \\parallel\\Events\\Input, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, setInput)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *input;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(input, php_parallel_events_input_ce);
    ZEND_PARSE_PARAMETERS_END();

    if (Z_TYPE(events->input) == IS_OBJECT) {
        zval_ptr_dtor(&events->input);
    }

    ZVAL_COPY(&events->input, input);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_add_channel_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, channel, \\parallel\\Channel, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, addChannel)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *target = NULL;
    zend_string *key = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(target, php_parallel_channel_ce)
    ZEND_PARSE_PARAMETERS_END();

    if (!php_parallel_events_add(events, NULL, target, &key)) {
        php_parallel_exception_ex(
            php_parallel_events_error_existence_ce,
            "target named \"%s\" already added",
            ZSTR_VAL(key));
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_add_future_arginfo, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_OBJ_INFO(0, future, \\parallel\\Future, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, addFuture)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *target = NULL;
    zend_string *name = NULL;
    zend_string *key = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(name)
        Z_PARAM_OBJECT_OF_CLASS(target, php_parallel_future_ce)
    ZEND_PARSE_PARAMETERS_END();

    if (!php_parallel_events_add(events, name, target, &key)) {
        php_parallel_exception_ex(
            php_parallel_events_error_existence_ce,
            "target named \"%s\" already added",
            ZSTR_VAL(key));
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_remove_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, target, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, remove)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_string *target = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(target)
    ZEND_PARSE_PARAMETERS_END();

    if (!php_parallel_events_remove(events, target)) {
        php_parallel_exception_ex(
            php_parallel_events_error_existence_ce,
            "target named \"%s\" not found",
            ZSTR_VAL(target));
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_set_timeout_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, setTimeout)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_long timeout = -1;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (!events->blocking) {
        php_parallel_exception_ex(
            php_parallel_events_error_ce,
            "cannot set timeout on loop in non-blocking mode");
        return;
    }

    events->timeout = timeout;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(php_parallel_events_set_blocking_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, blocking, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, setBlocking)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_bool blocking;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(blocking)
    ZEND_PARSE_PARAMETERS_END();

    if (events->timeout > -1) {
        php_parallel_exception_ex(
            php_parallel_events_error_ce,
            "cannot set blocking mode on loop with timeout");
        return;
    }

    events->blocking = blocking;
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(php_parallel_events_poll_arginfo, 0, 0, \\parallel\\Events\\Event, 1)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, poll)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    php_parallel_events_poll(events, return_value);
}

PHP_METHOD(Events, count)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());

    PARALLEL_PARAMETERS_NONE(return);

    RETURN_LONG(zend_hash_num_elements(&events->targets));
}

zend_function_entry php_parallel_events_methods[] = {
    PHP_ME(Events, setInput,    php_parallel_events_set_input_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addChannel,  php_parallel_events_add_channel_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addFuture,   php_parallel_events_add_future_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, remove,      php_parallel_events_remove_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, setBlocking, php_parallel_events_set_blocking_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, setTimeout,  php_parallel_events_set_timeout_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, poll,        php_parallel_events_poll_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, count,       NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PARALLEL_EVENTS)
{
    zend_class_entry ce;

    memcpy(
        &php_parallel_events_handlers,
        php_parallel_standard_handlers(),
        sizeof(zend_object_handlers));

    php_parallel_events_handlers.offset = XtOffsetOf(php_parallel_events_t, std);
    php_parallel_events_handlers.free_obj = php_parallel_events_destroy;

    INIT_NS_CLASS_ENTRY(ce, "parallel", "Events", php_parallel_events_methods);

    php_parallel_events_ce = zend_register_internal_class(&ce);
    php_parallel_events_ce->create_object = php_parallel_events_create;
    php_parallel_events_ce->get_iterator  = php_parallel_events_loop_create;
    php_parallel_events_ce->ce_flags |= ZEND_ACC_FINAL;

    php_parallel_events_ce->serialize = zend_class_serialize_deny;
    php_parallel_events_ce->unserialize = zend_class_unserialize_deny;

    zend_class_implements(php_parallel_events_ce, 2, zend_ce_countable, zend_ce_traversable);

    PHP_MINIT(PARALLEL_EVENTS_EVENT)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_EVENTS_INPUT)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS)
{
    PHP_MSHUTDOWN(PARALLEL_EVENTS_EVENT)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_EVENTS_INPUT)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}
#endif
