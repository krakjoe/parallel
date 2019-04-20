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
#include "handlers.h"
#include "future.h"
#include "channel.h"
#include "input.h"
#include "event.h"
#include "poll.h"
#include "loop.h"

#include "zend_interfaces.h"

zend_class_entry* php_parallel_events_ce;
zend_class_entry* php_parallel_events_timeout_ce;
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
    
    ZVAL_UNDEF(&events->input);
    
    return &events->std;
}

static zend_always_inline zend_bool php_parallel_events_add(php_parallel_events_t *events, zend_string *name, zval *object, zend_string **key) {    
    if(instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
        php_parallel_channel_t *channel = 
            (php_parallel_channel_t*)
                 php_parallel_channel_from(object);
            
        name = php_parallel_link_name(channel->link);
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

PHP_METHOD(Events, setInput)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *input;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_OBJECT_OF_CLASS(input, php_parallel_events_input_ce);
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected Input");
        return;
    );
    
    if (Z_TYPE(events->input) == IS_OBJECT) {
        zval_ptr_dtor(&events->input);
    }
    
    ZVAL_COPY(&events->input, input);
}

PHP_METHOD(Events, addChannel)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *target = NULL;
    zend_string *key = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_OBJECT_OF_CLASS(target, php_parallel_channel_ce)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected \\parallel\\Channel");
        return;
    );
    
    if (!php_parallel_events_add(events, NULL, target, &key)) {
        php_parallel_exception(
            "target named \"%s\" already added", 
            ZSTR_VAL(key));
    }
}

PHP_METHOD(Events, addFuture)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *target = NULL;
    zend_string *name = NULL;
    zend_string *key = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 2, 2)
        Z_PARAM_STR(name)
        Z_PARAM_OBJECT_OF_CLASS(target, php_parallel_future_ce)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected target name and \\parallel\\Future");
        return;
    );
    
    if (!php_parallel_events_add(events, name, target, &key)) {
        php_parallel_exception(
            "target named \"%s\" already added", 
            ZSTR_VAL(key));
    }
}

PHP_METHOD(Events, remove)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_string *name = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception("expected target name");
        return;
    );
    
    if (!php_parallel_events_remove(events, name)) {
        php_parallel_exception(
            "target named \"%s\" not found", 
            ZSTR_VAL(name));
    }
}

PHP_METHOD(Events, setTimeout)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_long timeout = -1;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected timeout");
        return;
    );
    
    events->timeout = timeout;
}

PHP_METHOD(Events, poll)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 0)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "no arguments expected");
        return;
    );
    
    php_parallel_events_poll(events, return_value);
}

zend_function_entry php_parallel_events_methods[] = {
    PHP_ME(Events, setInput,    NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addChannel,  NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addFuture,   NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, remove,      NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, setTimeout,  NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, poll,        NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void php_parallel_events_startup(void) {
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
	
    zend_class_implements(php_parallel_events_ce, 1, zend_ce_traversable);
	
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Timeout", NULL);
	
	php_parallel_events_timeout_ce = zend_register_internal_class_ex(&ce, php_parallel_exception_ce);
	php_parallel_events_timeout_ce->ce_flags |= ZEND_ACC_FINAL;
	
	php_parallel_events_event_startup();
	php_parallel_events_input_startup();
}

void php_parallel_events_shutdown(void) {
    php_parallel_events_event_shutdown();
    php_parallel_events_input_shutdown();
}
#endif
