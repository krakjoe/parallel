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
#ifndef HAVE_PARALLEL_EXCEPTIONS
#define HAVE_PARALLEL_EXCEPTIONS

#include "parallel.h"

struct _php_parallel_exception_t {
    zval class;
    zval file;
    zval line;
    zval code;
    zval message;
    zval trace;
    zval previous;
    const zend_object_handlers *handlers;
};

zend_class_entry* php_parallel_error_ce;
zend_class_entry* php_parallel_error_invalid_arguments_ce;

zend_class_entry* php_parallel_runtime_error_ce;
zend_class_entry* php_parallel_runtime_error_bootstrap_ce;
zend_class_entry* php_parallel_runtime_error_closed_ce;
zend_class_entry* php_parallel_runtime_error_killed_ce;
zend_class_entry* php_parallel_runtime_error_illegal_function_ce;
zend_class_entry* php_parallel_runtime_error_illegal_instruction_ce;
zend_class_entry* php_parallel_runtime_error_illegal_variable_ce;
zend_class_entry* php_parallel_runtime_error_illegal_parameter_ce;
zend_class_entry* php_parallel_runtime_error_illegal_type_ce;
zend_class_entry* php_parallel_runtime_error_illegal_return_ce;

zend_class_entry* php_parallel_future_error_ce;
zend_class_entry* php_parallel_future_error_killed_ce;
zend_class_entry* php_parallel_future_error_cancelled_ce;
zend_class_entry* php_parallel_future_error_foreign_ce;

zend_class_entry* php_parallel_channel_error_ce;
zend_class_entry* php_parallel_channel_error_existence_ce;
zend_class_entry* php_parallel_channel_error_illegal_value_ce;
zend_class_entry* php_parallel_channel_error_closed_ce;

zend_class_entry* php_parallel_sync_error_ce;
zend_class_entry* php_parallel_sync_error_illegal_value_ce;
zend_class_entry* php_parallel_sync_error_illegal_type_ce;
zend_class_entry* php_parallel_sync_error_illegal_offset_ce;
zend_class_entry* php_parallel_sync_error_illegal_access_ce;

zend_class_entry* php_parallel_events_error_ce;
zend_class_entry* php_parallel_events_error_existence_ce;
zend_class_entry* php_parallel_events_error_timeout_ce;

zend_class_entry* php_parallel_events_input_error_ce;
zend_class_entry* php_parallel_events_input_error_existence_ce;
zend_class_entry* php_parallel_events_input_error_illegal_value_ce;

zend_class_entry* php_parallel_events_event_error_ce;

static zend_always_inline zval* php_parallel_exceptions_read(zend_object *exception, zend_string *property) {
    zend_property_info *info;
    zend_class_entry *scope = EG(fake_scope);

    EG(fake_scope) = zend_ce_error;

    info = zend_get_property_info(zend_ce_error, property, 1);

    EG(fake_scope) = scope;

    return OBJ_PROP(exception, info->offset);
}

static zend_always_inline void php_parallel_exceptions_write(zend_object *exception, zend_string *property, zval *value) {
    zend_property_info *info;
    zend_class_entry *scope = EG(fake_scope);
    zval *slot;

    EG(fake_scope) = zend_ce_error;

    info = zend_get_property_info(zend_ce_error, property, 1);

    slot = OBJ_PROP(exception, info->offset);

    if (Z_REFCOUNTED_P(slot)) {
        zval_ptr_dtor(slot);
    }

    ZVAL_COPY_VALUE(slot, value);

    EG(fake_scope) = scope;
}

void php_parallel_exceptions_destroy(php_parallel_exception_t *ex) {
    PARALLEL_ZVAL_DTOR(&ex->class);
    PARALLEL_ZVAL_DTOR(&ex->file);
    PARALLEL_ZVAL_DTOR(&ex->line);
    PARALLEL_ZVAL_DTOR(&ex->message);
    PARALLEL_ZVAL_DTOR(&ex->code);
    PARALLEL_ZVAL_DTOR(&ex->trace);
    PARALLEL_ZVAL_DTOR(&ex->previous);

    pefree(ex, 1);
}

void php_parallel_exceptions_save(zval *saved, zend_object *exception) {
    zval class,
         *file     = php_parallel_exceptions_read(exception, ZSTR_KNOWN(ZEND_STR_FILE)),
         *line     = php_parallel_exceptions_read(exception, ZSTR_KNOWN(ZEND_STR_LINE)),
         *message  = php_parallel_exceptions_read(exception, ZSTR_KNOWN(ZEND_STR_MESSAGE)),
         *code     = php_parallel_exceptions_read(exception, ZSTR_KNOWN(ZEND_STR_CODE)),
         *trace    = php_parallel_exceptions_read(exception, ZSTR_KNOWN(ZEND_STR_TRACE)),
         previous;

    php_parallel_exception_t *ex =
        (php_parallel_exception_t*)
            pecalloc(1, sizeof(php_parallel_exception_t), 1);

    /* todo */
    ZVAL_NULL(&previous);

    ZVAL_STR(&class, exception->ce->name);

    PARALLEL_ZVAL_COPY(&ex->class,   &class,     1);
    PARALLEL_ZVAL_COPY(&ex->file,    file,       1);
    PARALLEL_ZVAL_COPY(&ex->line,    line,       1);
    PARALLEL_ZVAL_COPY(&ex->message, message,    1);
    PARALLEL_ZVAL_COPY(&ex->code,    code,       1);
    PARALLEL_ZVAL_COPY(&ex->trace,   trace,      1);
    PARALLEL_ZVAL_COPY(&ex->previous, &previous, 1);

    ex->handlers = exception->handlers;

    ZVAL_PTR(saved, ex);

    zend_clear_exception();
}

zend_object* php_parallel_exceptions_restore(zval *exception) {
    zend_object      *object;
    zend_class_entry *type;
    zval file, line, message, code, trace, previous;

    php_parallel_exception_t *ex =
        (php_parallel_exception_t*) Z_PTR_P(exception);

    PARALLEL_ZVAL_COPY(&file,     &ex->file,     0);
    PARALLEL_ZVAL_COPY(&line,     &ex->line,     0);
    PARALLEL_ZVAL_COPY(&message,  &ex->message,  0);
    PARALLEL_ZVAL_COPY(&code,     &ex->code,     0);
    PARALLEL_ZVAL_COPY(&trace,    &ex->trace,    0);
    PARALLEL_ZVAL_COPY(&previous, &ex->previous, 0);

    type = zend_lookup_class(Z_STR(ex->class));

    if (!type) {
        /* we lost type info */
        type = php_parallel_future_error_foreign_ce;
    }

    object = zend_objects_new(type);
    object->handlers = ex->handlers;
    object_properties_init(object, type);

    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_FILE),     &file);
    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_LINE),     &line);
    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_MESSAGE),  &message);
    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_CODE),     &code);
    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_TRACE),    &trace);
    php_parallel_exceptions_write(object, ZSTR_KNOWN(ZEND_STR_PREVIOUS), &previous);

    return object;
}

PHP_MINIT_FUNCTION(PARALLEL_EXCEPTIONS)
{
    zend_class_entry ce;

    /*
    * Base Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel", "Error", NULL);
    php_parallel_error_ce =
        zend_register_internal_class_ex(&ce, zend_ce_error);

    /*
    * Runtime Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime", "Error", NULL);
    php_parallel_runtime_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Bootstrap", NULL);
    php_parallel_runtime_error_bootstrap_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Closed", NULL);
    php_parallel_runtime_error_closed_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Killed", NULL);
    php_parallel_runtime_error_killed_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalFunction", NULL);
    php_parallel_runtime_error_illegal_function_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalVariable", NULL);
    php_parallel_runtime_error_illegal_variable_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalParameter", NULL);
    php_parallel_runtime_error_illegal_parameter_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalInstruction", NULL);
    php_parallel_runtime_error_illegal_instruction_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalReturn", NULL);
    php_parallel_runtime_error_illegal_return_ce =
        zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

    /*
    * Future Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Future", "Error", NULL);
    php_parallel_future_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Future\\Error", "Killed", NULL);
    php_parallel_future_error_killed_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Future\\Error", "Cancelled", NULL);
    php_parallel_future_error_cancelled_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Future\\Error", "Foreign", NULL);
    php_parallel_future_error_foreign_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    /*
    * Channel Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel", "Error", NULL);
    php_parallel_channel_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "Existence", NULL);
    php_parallel_channel_error_existence_ce =
        zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "IllegalValue", NULL);
    php_parallel_channel_error_illegal_value_ce =
        zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "Closed", NULL);
    php_parallel_channel_error_closed_ce =
        zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);

    /*
    * Sync Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Sync", "Error", NULL);
    php_parallel_sync_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Sync\\Error", "IllegalValue", NULL);
    php_parallel_sync_error_illegal_value_ce =
        zend_register_internal_class_ex(&ce, php_parallel_sync_error_ce);

    /*
    * Events Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Error", NULL);
    php_parallel_events_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Error", "Existence", NULL);
    php_parallel_events_error_existence_ce =
        zend_register_internal_class_ex(&ce, php_parallel_events_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Error", "Timeout", NULL);
    php_parallel_events_error_timeout_ce =
        zend_register_internal_class_ex(&ce, php_parallel_events_error_ce);

    /*
    * Input Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Input", "Error", NULL);
    php_parallel_events_input_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Input\\Error", "Existence", NULL);
    php_parallel_events_input_error_existence_ce =
        zend_register_internal_class_ex(&ce, php_parallel_events_input_error_ce);

    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Input\\Error", "IllegalValue", NULL);
    php_parallel_events_input_error_illegal_value_ce =
        zend_register_internal_class_ex(&ce, php_parallel_events_input_error_ce);

    /*
    * Event Exceptions
    */
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Event", "Error", NULL);
    php_parallel_events_event_error_ce =
        zend_register_internal_class_ex(&ce, php_parallel_error_ce);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_EXCEPTIONS)
{
    return SUCCESS;
}
#endif
