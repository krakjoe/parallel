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
#include "channel.h"
#include "future.h"
#include "event.h"
#include "payloads.h"

#include "ext/standard/php_mt_rand.h"

zend_class_entry* php_parallel_events_ce;
zend_class_entry* php_parallel_events_timeout_ce;
zend_object_handlers php_parallel_events_handlers;

typedef enum {
    PHP_PARALLEL_EVENTS_OK = 0,
    PHP_PARALLEL_EVENTS_NOT_OK,
    PHP_PARALLEL_EVENTS_NOT_FOUND,
    PHP_PARALLEL_EVENTS_NOT_NAMED,
    PHP_PARALLEL_EVENTS_NOT_UNIQUE
} php_parallel_events_return;

typedef enum {
    PHP_PARALLEL_EVENTS_LINK = 1,
    PHP_PARALLEL_EVENTS_FUTURE
} php_parallel_events_type_t;

typedef struct _php_parallel_events_state_t {
    php_parallel_events_type_t type;
    zend_string *name;
    zend_bool readable;
    zend_bool writable;
    zend_object *object;
} php_parallel_events_state_t;

#define php_parallel_events_state_initializer {0, NULL, 0, 0, NULL}

void php_parallel_events_state_free(zval *zv) {
    efree(Z_PTR_P(zv));
}

static zend_object* php_parallel_events_create(zend_class_entry *type) {
    php_parallel_events_t *events = 
        (php_parallel_events_t*) ecalloc(1, 
            sizeof(php_parallel_events_t) + zend_object_properties_size(type));
            
    zend_object_std_init(&events->std, type);
    
    events->std.handlers = &php_parallel_events_handlers;
    
    zend_hash_init(&events->set, 32, NULL, ZVAL_PTR_DTOR, 0);
    
    events->timeout = -1;
    
    return &events->std;
}

static php_parallel_events_return php_parallel_events_add(php_parallel_events_t *events, zend_string *name, zval *object, zend_string **key) {
    if (Z_TYPE_P(object) != IS_OBJECT) {
        /* ignore noise */
        return PHP_PARALLEL_EVENTS_OK;
    }
    
    if (instanceof_function(Z_OBJCE_P(object), php_parallel_future_ce)) {
        if (!name) {
            return PHP_PARALLEL_EVENTS_NOT_NAMED;
        }
    } else if(instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
        php_parallel_channel_t *channel = 
            (php_parallel_channel_t*)
                 php_parallel_channel_from(object);
            
        name = php_parallel_link_name(channel->link);
    } else {
        return PHP_PARALLEL_EVENTS_NOT_OK;
    }
    
    *key = name;
    
    if (!zend_hash_add(&events->set, name, object)) {
        return PHP_PARALLEL_EVENTS_NOT_UNIQUE;
    }
    
    Z_ADDREF_P(object);
    
    return PHP_PARALLEL_EVENTS_OK;
}

static php_parallel_events_return php_parallel_events_remove(php_parallel_events_t *events, zend_string *name) {
    zend_long size = 
        zend_hash_num_elements(&events->set);
    
    zend_hash_del(&events->set, name);
    
    if (size == zend_hash_num_elements(&events->set)) {
        return PHP_PARALLEL_EVENTS_NOT_FOUND;
    }
    
    return PHP_PARALLEL_EVENTS_OK;
}

static zend_always_inline zend_ulong php_parallel_events_wait_begin(php_parallel_events_t *events, zval *payloads) {
    zend_string *name;
    zval *object;
    
    zend_hash_init(
        &events->state, 
        zend_hash_num_elements(&events->set), 
        NULL, 
        php_parallel_events_state_free, 0);
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(&events->set, name, object) {
        php_parallel_events_state_t state = php_parallel_events_state_initializer;
        
        if (instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
            php_parallel_channel_t *channel = 
                php_parallel_channel_from(object);
            
            php_parallel_link_lock(channel->link);
            
            state.type     = PHP_PARALLEL_EVENTS_LINK;
            state.name     = name;
            
            if (php_parallel_events_payloads_exists(payloads, name)) {
                state.writable = php_parallel_link_writable(channel->link);   
            } else {
                state.readable = php_parallel_link_readable(channel->link);
            }
            
            state.object   = Z_OBJ_P(object);
            
            if (state.readable || state.writable) {
                zend_hash_next_index_insert_mem(
                    &events->state,
                    &state, 
                    sizeof(php_parallel_events_state_t));
            } else {
                php_parallel_link_unlock(channel->link);
            }
        } else {
            php_parallel_future_t *future = 
                php_parallel_future_from(object);
            
            php_parallel_future_lock(future);
            
            state.type     = PHP_PARALLEL_EVENTS_FUTURE;
            state.name     = name;
            state.readable = php_parallel_future_readable(future);
            state.object   = Z_OBJ_P(object);
            
            if (state.readable) {
                zend_hash_next_index_insert_mem(
                    &events->state,
                    &state, 
                    sizeof(php_parallel_events_state_t));
            } else {
                php_parallel_future_unlock(future);
            }
        }
    } ZEND_HASH_FOREACH_END();
    
    return zend_hash_num_elements(&events->state);
}

static zend_always_inline void php_parallel_events_wait_end(php_parallel_events_t *events) {
    php_parallel_events_state_t *state;
    
    ZEND_HASH_FOREACH_PTR(&events->state, state) {
        if (state->type == PHP_PARALLEL_EVENTS_LINK) {
            php_parallel_channel_t *channel = 
                php_parallel_channel_fetch(state->object);

            php_parallel_link_unlock(channel->link);
        } else {
            php_parallel_future_t *future = 
                php_parallel_future_fetch(state->object);
            
            php_parallel_future_unlock(future);
        }
    } ZEND_HASH_FOREACH_END();
    
    zend_hash_destroy(&events->state);
}

static zend_always_inline zend_bool php_parallel_events_wait_link(
                            php_parallel_events_t *events, 
                            php_parallel_events_state_t *state, 
                            zval *payloads,
                            zval *retval) {
    php_parallel_channel_t *channel = 
        php_parallel_channel_fetch(state->object);
    zval *payload;

    if ((payload = php_parallel_events_payloads_find(payloads, state->name))) {
        
        if (state->writable) {
        
            if (php_parallel_link_send(channel->link, payload)) {
                
                php_parallel_events_event_construct(
                    events,
                    PHP_PARALLEL_EVENTS_EVENT_WRITE,
                    state->name,
                    state->object,
                    NULL,
                    retval);
                
                php_parallel_events_payloads_remove(payloads, state->name);
                
                return 1;
            }
        }
    } else {
        if (state->readable) {
            zval read;
            
            if (php_parallel_link_recv(channel->link, &read)) {
                php_parallel_events_event_construct(
                    events,
                    PHP_PARALLEL_EVENTS_EVENT_READ,
                    state->name,
                    state->object,
                    &read,
                    retval);
                
                return 1;
            }
        }
    }
    
    return 0;
}

static zend_always_inline zend_bool php_parallel_events_wait_future(
                            php_parallel_events_t *events, 
                            php_parallel_events_state_t *state, 
                            zval *retval) {
    if (state->readable) {
        zval read;
        
        php_parallel_future_t *future =
            php_parallel_future_fetch(state->object);
            
        php_parallel_future_value(future, &read);
        
        php_parallel_events_event_construct(
            events,
            PHP_PARALLEL_EVENTS_EVENT_READ,
            state->name,
            state->object,
            &read,
            retval);
        
        return 1;
    }
    
    return 0;
}

static void php_parallel_events_wait(php_parallel_events_t *events, zval *payloads, zval *retval) {
    struct timeval  now,
                    stop;
    
    if (zend_hash_num_elements(&events->set) == 0) {
        ZVAL_FALSE(retval);
        return;
    }
    
    if (events->timeout > -1 && gettimeofday(&stop, NULL) == SUCCESS) {
        stop.tv_sec += (events->timeout / 1000000L);
	    stop.tv_sec += (stop.tv_usec + (events->timeout % 1000000L)) / 1000000L;
	    stop.tv_usec = (stop.tv_usec + (events->timeout % 1000000L)) % 1000000L;
    }

    do {
        zval *zv;
        php_parallel_events_state_t *state;
        zend_long selected;
        zend_long size = 
            php_parallel_events_wait_begin(events, payloads);

        if (size == 0) {
            if (events->timeout > -1 && gettimeofday(&now, NULL) == SUCCESS) {
                 if (now.tv_sec >= stop.tv_sec &&
                     now.tv_usec >= stop.tv_usec) {
                        php_parallel_exception_ex(
                            php_parallel_events_timeout_ce, 
                            "timeout occured");
                        break;
                 }
                 
                 usleep(events->timeout / 1000);
            }
            
_php_parallel_events_wait_continue:
            php_parallel_events_wait_end(events);
            continue;
        }
        
        selected = php_mt_rand_range(
            0, 
            size - 1);
            
        ZEND_HASH_INDEX_FIND(
            (&events->state), 
            (selected), 
            (zv), 
            _php_parallel_events_wait_continue);
        
        state = Z_PTR_P(zv);
        
        if (state->type == PHP_PARALLEL_EVENTS_LINK) {
            if (php_parallel_events_wait_link(
                    events, 
                    state,
                    payloads,
                    retval)) {
                break;        
            }
        } else {
            if (php_parallel_events_wait_future(
                    events, 
                    state, 
                    retval)) {
                break;        
            }
        }
        
        php_parallel_events_wait_end(events);  
    } while (1);
    
    php_parallel_events_wait_end(events);  
}

static void php_parallel_events_destroy(zend_object *zo) {
    php_parallel_events_t *events =
        php_parallel_events_fetch(zo);
        
    zend_hash_destroy(&events->set);
    
    zend_object_std_dtor(zo);
}

PHP_METHOD(Events, addTargetChannel)
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
    
    switch (php_parallel_events_add(events, NULL, target, &key)) {
        case PHP_PARALLEL_EVENTS_NOT_UNIQUE:
            php_parallel_exception("target named \"%s\" already added", ZSTR_VAL(key));
            return;
        
        case PHP_PARALLEL_EVENTS_OK:
            break;
     
        EMPTY_SWITCH_DEFAULT_CASE();     
    }
}

PHP_METHOD(Events, addTargetFuture)
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
    
    switch (php_parallel_events_add(events, name, target, &key)) {
        case PHP_PARALLEL_EVENTS_NOT_UNIQUE:
            php_parallel_exception("target named \"%s\" already added", ZSTR_VAL(key));
            return;
        
        case PHP_PARALLEL_EVENTS_OK:
            break;
     
        EMPTY_SWITCH_DEFAULT_CASE();
    }
}

PHP_METHOD(Events, removeTarget)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zend_string *name = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception("expected string");
        return;
    );
    
    switch (php_parallel_events_remove(events, name)) {
        case PHP_PARALLEL_EVENTS_OK:
            break;
            
        case PHP_PARALLEL_EVENTS_NOT_FOUND:
            php_parallel_exception(
                "target named \"%s\" not found", ZSTR_VAL(name));
            return;
            
        EMPTY_SWITCH_DEFAULT_CASE();
    }
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_events_set_wait_timeout_arginfo, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, setWaitTimeout)
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

ZEND_BEGIN_ARG_INFO_EX(php_parallel_events_wait_arginfo, 0, 0, 0)
	ZEND_ARG_OBJ_INFO(0, payloads, \\parallel\\Events\\Payloads, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Events, wait)
{
    php_parallel_events_t *events = php_parallel_events_from(getThis());
    zval *payloads = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_OBJECT_OF_CLASS(payloads, php_parallel_events_payloads_ce)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected optional payloads");
        return;
    );
    
    if (zend_hash_num_elements(&events->set) == 0) {
        RETURN_FALSE;
    }
    
    php_parallel_events_wait(events, payloads, return_value);
}

zend_function_entry php_parallel_events_methods[] = {
    PHP_ME(Events, addTargetChannel, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addTargetFuture, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, removeTarget, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, setWaitTimeout, php_parallel_events_set_wait_timeout_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Events, wait, php_parallel_events_wait_arginfo, ZEND_ACC_PUBLIC)
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
	php_parallel_events_ce->ce_flags |= ZEND_ACC_FINAL;
	
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Timeout", NULL);
	
	php_parallel_events_timeout_ce = zend_register_internal_class_ex(&ce, php_parallel_exception_ce);
	php_parallel_events_timeout_ce->ce_flags |= ZEND_ACC_FINAL;
	
	php_parallel_events_event_startup();
	php_parallel_events_payloads_startup();
}

void php_parallel_events_shutdown(void) {
    php_parallel_events_event_shutdown();
    php_parallel_events_payloads_shutdown();
}
#endif
