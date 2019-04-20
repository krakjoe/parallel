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

static zend_always_inline zend_bool php_parallel_events_poll_random(php_parallel_events_t *events, zend_string **name, zend_object **object) {
    uint32_t  size = events->targets.nNumUsed;
    zend_long  random;
    
    if (size == 0) {
        return 0;
    }
    
    random = php_mt_rand_range(0, (zend_long) size - 1);
    
    do {
        Bucket *bucket = &events->targets.arData[random];
        
        if (!Z_ISUNDEF(bucket->val)) {
            *name   = bucket->key;
            *object = Z_OBJ(bucket->val);

            return 1;
        }
        
        random = php_mt_rand_range(0, (zend_long) size - 1);
    } while(1);
    
    return 0;
}

static zend_always_inline php_parallel_events_state_t* php_parallel_events_poll_begin(php_parallel_events_t *events, php_parallel_events_state_t *state) {
    zend_string *name;
    zend_object *object;
    
    if (!php_parallel_events_poll_random(events, &name, &object)) {
        return NULL;
    }
    
    memset(state, 0, sizeof(php_parallel_events_state_t));
    
    if (instanceof_function(object->ce, php_parallel_channel_ce)) {
        php_parallel_channel_t *channel = 
            php_parallel_channel_fetch(object);
        
        php_parallel_link_lock(channel->link);
        
        state->type     = PHP_PARALLEL_EVENTS_LINK;
        state->name     = name;
        
        if (php_parallel_events_input_exists(&events->input, name)) {
            state->writable = php_parallel_link_writable(channel->link);   
        } else {
            state->readable = php_parallel_link_readable(channel->link);
        }
        
        state->object   = object;
        
        if (state->readable || state->writable) {
            return state;
        } else {
            php_parallel_link_unlock(channel->link);
        }
    } else {
        php_parallel_future_t *future = 
            php_parallel_future_fetch(object);
        
        php_parallel_future_lock(future);
        
        state->type     = PHP_PARALLEL_EVENTS_FUTURE;
        state->name     = name;
        state->readable = php_parallel_future_readable(future);
        state->object   = object;
        
        if (state->readable) {
            return state;
        } else {
            php_parallel_future_unlock(future);
        }
    }
    
    return NULL;
}

static zend_always_inline void php_parallel_events_poll_end(php_parallel_events_state_t *state) {
    if (!state) {
        return;
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
    php_parallel_events_state_t stack, 
                               *selected;
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
        selected = php_parallel_events_poll_begin(events, &stack);
        
        if (selected == NULL) {
            if (events->timeout > -1 && gettimeofday(&now, NULL) == SUCCESS) {
                 if (now.tv_sec >= stop.tv_sec &&
                     now.tv_usec >= stop.tv_usec) {
                    php_parallel_exception_ex(
                        php_parallel_events_timeout_ce, 
                            "timeout occured");
                    return;
                 }
                 
                 usleep(events->timeout / 1000);
            }

            continue;
        }
        
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
        
        php_parallel_events_poll_end(selected);  
    } while (1);

    php_parallel_events_poll_end(selected);
}
#endif
