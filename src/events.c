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
#include "input.h"

#include "zend_interfaces.h"

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

static void php_parallel_events_state_free(zval *zv) {
    efree(Z_PTR_P(zv));
}

static zend_object* php_parallel_events_create(zend_class_entry *type) {
    php_parallel_events_t *events = 
        (php_parallel_events_t*) ecalloc(1, 
            sizeof(php_parallel_events_t) + zend_object_properties_size(type));
            
    zend_object_std_init(&events->std, type);
    
    events->std.handlers = &php_parallel_events_handlers;
    
    zend_hash_init(&events->targets, 32, NULL, ZVAL_PTR_DTOR, 0);
    
    events->timeout = -1;
    
    ZVAL_UNDEF(&events->input);
    
    return &events->std;
}

static zend_always_inline php_parallel_events_return php_parallel_events_add(php_parallel_events_t *events, zend_string *name, zval *object, zend_string **key) {
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
    
    if (!zend_hash_add(&events->targets, name, object)) {
        return PHP_PARALLEL_EVENTS_NOT_UNIQUE;
    }
    
    Z_ADDREF_P(object);
    
    return PHP_PARALLEL_EVENTS_OK;
}

static zend_always_inline php_parallel_events_return php_parallel_events_remove(php_parallel_events_t *events, zend_string *name) {
    zend_long size = 
        zend_hash_num_elements(&events->targets);
    
    zend_hash_del(&events->targets, name);
    
    if (size == zend_hash_num_elements(&events->targets)) {
        return PHP_PARALLEL_EVENTS_NOT_FOUND;
    }
    
    return PHP_PARALLEL_EVENTS_OK;
}

static zend_always_inline zend_ulong php_parallel_events_poll_begin(php_parallel_events_t *events) {
    zend_string *name;
    zval *object;
    
    zend_hash_init(
        &events->state, 
        zend_hash_num_elements(&events->targets), 
        NULL, 
        php_parallel_events_state_free, 0);
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(&events->targets, name, object) {
        php_parallel_events_state_t state = php_parallel_events_state_initializer;
        
        if (instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
            php_parallel_channel_t *channel = 
                php_parallel_channel_from(object);
            
            php_parallel_link_lock(channel->link);
            
            state.type     = PHP_PARALLEL_EVENTS_LINK;
            state.name     = name;
            
            if (php_parallel_events_input_exists(&events->input, name)) {
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

static int php_parallel_events_poll_unlock(zval *zv, void *arg) {
    php_parallel_events_state_t *state = Z_PTR_P(zv),
                                *selected = arg;

    if (selected && state == selected) {
        return ZEND_HASH_APPLY_KEEP;
    }
    
    if (state->type == PHP_PARALLEL_EVENTS_LINK) {
        php_parallel_channel_t *channel = 
            php_parallel_channel_fetch(state->object);

        php_parallel_link_unlock(channel->link);
    } else {
        php_parallel_future_t *future = 
            php_parallel_future_fetch(state->object);
        
        php_parallel_future_unlock(future);
    }
    
    return ZEND_HASH_APPLY_REMOVE;
}

static zend_always_inline php_parallel_events_state_t* php_parallel_events_poll_select(php_parallel_events_t *events) {
    php_parallel_events_state_t *selected;
    
    zend_long idx = php_mt_rand_range(
        0, 
        zend_hash_num_elements(&events->state) - 1);

    selected = 
        (php_parallel_events_state_t*)
            zend_hash_index_find_ptr(&events->state, idx);
    
    zend_hash_apply_with_argument(
        &events->state, 
        php_parallel_events_poll_unlock, selected);
        
    return selected;
}

static zend_always_inline void php_parallel_events_poll_end(php_parallel_events_t *events) {
    zend_hash_apply_with_argument(
        &events->state, 
        php_parallel_events_poll_unlock, NULL);
    
    zend_hash_destroy(&events->state);
}

static zend_always_inline zend_bool php_parallel_events_poll_link(
                            php_parallel_events_t *events, 
                            php_parallel_events_state_t *state, 
                            zval *retval) {
    php_parallel_channel_t *channel = 
        php_parallel_channel_fetch(state->object);
    zval *input;

    if ((input = php_parallel_events_input_find(&events->input, state->name))) {
        
        if (state->writable) {
        
            if (php_parallel_link_send(channel->link, input)) {

                php_parallel_events_event_construct(
                    events,
                    PHP_PARALLEL_EVENTS_EVENT_WRITE,
                    state->name,
                    state->object,
                    NULL,
                    retval);
                
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

static zend_always_inline zend_bool php_parallel_events_poll_future(
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

static zend_always_inline void php_parallel_events_poll(php_parallel_events_t *events, zval *retval) {
    struct timeval  now,
                    stop;
    
    if (zend_hash_num_elements(&events->targets) == 0) {
        ZVAL_FALSE(retval);
        return;
    }
    
    if (events->timeout > -1 && gettimeofday(&stop, NULL) == SUCCESS) {
        stop.tv_sec += (events->timeout / 1000000L);
	    stop.tv_sec += (stop.tv_usec + (events->timeout % 1000000L)) / 1000000L;
	    stop.tv_usec = (stop.tv_usec + (events->timeout % 1000000L)) % 1000000L;
    }

    do {
        php_parallel_events_state_t *selected;
        zend_long size = 
            php_parallel_events_poll_begin(events);

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
            
            php_parallel_events_poll_end(events);
            continue;
        }
        
        selected = php_parallel_events_poll_select(events);
        
        if (selected->type == PHP_PARALLEL_EVENTS_LINK) {
            if (php_parallel_events_poll_link(
                    events, 
                    selected,
                    retval)) {
                break;        
            }
        } else {
            if (php_parallel_events_poll_future(
                    events, 
                    selected, 
                    retval)) {
                break;        
            }
        }
        
        php_parallel_events_poll_end(events);  
    } while (1);
    
    php_parallel_events_poll_end(events);  
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
    
    switch (php_parallel_events_add(events, NULL, target, &key)) {
        case PHP_PARALLEL_EVENTS_NOT_UNIQUE:
            php_parallel_exception("target named \"%s\" already added", ZSTR_VAL(key));
            return;
        
        case PHP_PARALLEL_EVENTS_OK:
            break;
     
        EMPTY_SWITCH_DEFAULT_CASE();     
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
    
    switch (php_parallel_events_add(events, name, target, &key)) {
        case PHP_PARALLEL_EVENTS_NOT_UNIQUE:
            php_parallel_exception("target named \"%s\" already added", ZSTR_VAL(key));
            return;
        
        case PHP_PARALLEL_EVENTS_OK:
            break;
     
        EMPTY_SWITCH_DEFAULT_CASE();
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

zend_function_entry php_parallel_events_methods[] = {
    PHP_ME(Events, setInput,    NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addChannel,  NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, addFuture,   NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, remove,      NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Events, setTimeout,  NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

typedef struct _php_parallel_events_iterator_t {
    zend_object_iterator it;
    zval events;
    zval event;
} php_parallel_events_iterator_t;

static void php_parallel_events_iterator_destroy(zend_object_iterator *zo) {
    php_parallel_events_iterator_t *iterator =
        (php_parallel_events_iterator_t*) zo;
    
    if (!Z_ISUNDEF(iterator->event)) {
        zval_ptr_dtor(&iterator->event);
    }
    
    zval_ptr_dtor(&iterator->events);
}

static int php_parallel_events_iterator_valid(zend_object_iterator *zo) {
    php_parallel_events_iterator_t *iterator =
        (php_parallel_events_iterator_t*) zo;

    return Z_TYPE(iterator->event) == IS_OBJECT ? SUCCESS : FAILURE;
}

static void php_parallel_events_iterator_poll(zend_object_iterator *zo) {
    php_parallel_events_iterator_t *iterator =
        (php_parallel_events_iterator_t*) zo;
    php_parallel_events_t *events =
        php_parallel_events_from(&iterator->events);

    if (Z_TYPE(iterator->event) == IS_OBJECT) {
        zval_ptr_dtor(&iterator->event);
    }

    php_parallel_events_poll(events, &iterator->event);
}

static zval* php_parallel_events_iterator_current(zend_object_iterator *zo) {
    php_parallel_events_iterator_t *iterator =
        (php_parallel_events_iterator_t*) zo;
    
    return &iterator->event;
}

zend_object_iterator_funcs php_parallel_events_iterator_functions = {
    .dtor               = php_parallel_events_iterator_destroy,
    .valid              = php_parallel_events_iterator_valid,
    .move_forward       = php_parallel_events_iterator_poll,
    .get_current_data   = php_parallel_events_iterator_current,
    .rewind             = php_parallel_events_iterator_poll,
    .invalidate_current = NULL,
    .get_current_key    = NULL,
};

zend_object_iterator* php_parallel_events_iterator_create(zend_class_entry *type, zval *events, int by_ref) {
    php_parallel_events_iterator_t *iterator = 
        (php_parallel_events_iterator_t*) 
            ecalloc(1, sizeof(php_parallel_events_iterator_t));
    
    zend_iterator_init(&iterator->it);
    
    iterator->it.funcs = &php_parallel_events_iterator_functions;
    
    ZVAL_COPY(&iterator->events, events);
    
    ZVAL_UNDEF(&iterator->event);
    
    return &iterator->it;
}

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
	php_parallel_events_ce->get_iterator  = php_parallel_events_iterator_create;
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
