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
#ifndef HAVE_PARALLEL_EVENTS_POLL
#define HAVE_PARALLEL_EVENTS_POLL

#include "parallel.h"
#include "future.h"
#include "channel.h"
#include "events.h"
#include "input.h"
#include "event.h"

#include "ext/standard/php_mt_rand.h"

static void php_parallel_events_state_free(zval *zv) {
    efree(Z_PTR_P(zv));
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

void php_parallel_events_poll(php_parallel_events_t *events, zval *retval) {
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
#endif
