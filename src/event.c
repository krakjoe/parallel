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
#ifndef HAVE_PARALLEL_EVENTS_EVENT
#define HAVE_PARALLEL_EVENTS_EVENT

#include "parallel.h"

zend_class_entry* php_parallel_events_event_ce;
zend_class_entry* php_parallel_events_event_type_ce;

static zend_string *php_parallel_events_event_type;
static zend_string *php_parallel_events_event_source;
static zend_string *php_parallel_events_event_object;
static zend_string *php_parallel_events_event_value;

static uint32_t php_parallel_events_event_type_offset;
static uint32_t php_parallel_events_event_source_offset;
static uint32_t php_parallel_events_event_object_offset;
static uint32_t php_parallel_events_event_value_offset;

#define PHP_PARALLEL_EVENTS_EVENT_PROPERTY(t, p) \
    ZVAL_##t(OBJ_PROP(Z_OBJ_P(return_value), php_parallel_events_event_##p##_offset), p)

void php_parallel_events_event_construct(
        php_parallel_events_t *events,
        php_parallel_events_event_type_t type,
        zend_string *source,
        zend_object *object,
        zval *value,
        zval *return_value) {
    object_init_ex(
        return_value,
        php_parallel_events_event_ce);

    GC_ADDREF(object);

    PHP_PARALLEL_EVENTS_EVENT_PROPERTY(LONG,      type);
    PHP_PARALLEL_EVENTS_EVENT_PROPERTY(STR,       source);
    PHP_PARALLEL_EVENTS_EVENT_PROPERTY(OBJ,       object);

    if (type == PHP_PARALLEL_EVENTS_EVENT_WRITE) {
        php_parallel_events_input_remove(&events->input, source);
    } else if (type == PHP_PARALLEL_EVENTS_EVENT_READ ||
               type == PHP_PARALLEL_EVENTS_EVENT_ERROR) {
        PHP_PARALLEL_EVENTS_EVENT_PROPERTY(COPY_VALUE, value);
    }

    zend_hash_del(&events->targets, source);
}

PHP_METHOD(Event, __construct)
{
    php_parallel_exception_ex(
        php_parallel_events_event_error_ce,
        "construction of Events\\Event objects is not allowed");
}

zend_function_entry php_parallel_events_event_methods[] = {
    PHP_ME(Event, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static uint32_t php_parallel_events_event_offsetof(zend_string *property) {
    zend_property_info *info =
        zend_get_property_info(php_parallel_events_event_ce, property, 1);

    return info->offset;
}

PHP_MINIT_FUNCTION(PARALLEL_EVENTS_EVENT)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Event", php_parallel_events_event_methods);

    php_parallel_events_event_ce = zend_register_internal_class(&ce);
    php_parallel_events_event_ce->ce_flags |= ZEND_ACC_FINAL;

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Event", "Type", NULL);

    php_parallel_events_event_type_ce = zend_register_internal_class(&ce);
    php_parallel_events_event_type_ce->ce_flags |= ZEND_ACC_FINAL;

    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Read"),   PHP_PARALLEL_EVENTS_EVENT_READ);
    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Write"),  PHP_PARALLEL_EVENTS_EVENT_WRITE);
    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Close"),  PHP_PARALLEL_EVENTS_EVENT_CLOSE);
    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Cancel"), PHP_PARALLEL_EVENTS_EVENT_CANCEL);
    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Kill"),   PHP_PARALLEL_EVENTS_EVENT_KILL);
    zend_declare_class_constant_long(php_parallel_events_event_type_ce, ZEND_STRL("Error"),  PHP_PARALLEL_EVENTS_EVENT_ERROR);

    php_parallel_events_event_type =
        zend_new_interned_string(
            zend_string_init(ZEND_STRL("type"), 1));
    php_parallel_events_event_source =
        zend_new_interned_string(
            zend_string_init(ZEND_STRL("source"), 1));
    php_parallel_events_event_object =
        zend_new_interned_string(
            zend_string_init(ZEND_STRL("object"), 1));
    php_parallel_events_event_value =
        zend_new_interned_string(
            zend_string_init(ZEND_STRL("value"), 1));

    zend_declare_property_null(php_parallel_events_event_ce, ZEND_STRL("type"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(php_parallel_events_event_ce, ZEND_STRL("source"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(php_parallel_events_event_ce, ZEND_STRL("object"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(php_parallel_events_event_ce, ZEND_STRL("value"), ZEND_ACC_PUBLIC);

    php_parallel_events_event_type_offset =
        php_parallel_events_event_offsetof(php_parallel_events_event_type);
    php_parallel_events_event_source_offset =
        php_parallel_events_event_offsetof(php_parallel_events_event_source);
    php_parallel_events_event_object_offset =
        php_parallel_events_event_offsetof(php_parallel_events_event_object);
    php_parallel_events_event_value_offset =
        php_parallel_events_event_offsetof(php_parallel_events_event_value);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS_EVENT)
{
    zend_string_release(php_parallel_events_event_type);
    zend_string_release(php_parallel_events_event_source);
    zend_string_release(php_parallel_events_event_object);
    zend_string_release(php_parallel_events_event_value);

    return SUCCESS;
}
#endif
