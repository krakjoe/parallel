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

#include "ext/standard/php_mt_rand.h"

typedef struct _php_parallel_events_poll_t {
    struct timeval stop;
} php_parallel_events_poll_t;

static zend_always_inline zend_bool php_parallel_events_poll_init(php_parallel_events_poll_t *poll, php_parallel_events_t *events) {
    if (events->targets.nNumUsed == 0) {
        return 0;
    }

    if (events->timeout > -1) {
        if (gettimeofday(&poll->stop, NULL) == SUCCESS) {
            poll->stop.tv_sec += (events->timeout / 1000000L);
            poll->stop.tv_sec += (poll->stop.tv_usec + (events->timeout % 1000000L)) / 1000000L;
            poll->stop.tv_usec = (poll->stop.tv_usec + (events->timeout % 1000000L)) % 1000000L;
        }
        /* return 0 ? */
    }

    return 1;
}

static zend_always_inline zend_bool php_parallel_events_poll_timeout(php_parallel_events_poll_t *poll, php_parallel_events_t *events) {
    struct timeval now;

    if (events->timeout > -1 && gettimeofday(&now, NULL) == SUCCESS) {
         if (now.tv_sec >= poll->stop.tv_sec &&
             now.tv_usec >= poll->stop.tv_usec) {
             php_parallel_exception_ex(
                php_parallel_events_error_timeout_ce,
                "timeout occured");
            return 1;
         }
    }

    return 0;
}

static zend_always_inline zend_bool php_parallel_events_poll_random(php_parallel_events_t *events, zend_string **name, zend_object **object) {
    uint32_t  size = events->targets.nNumUsed;
    zend_long random =
        php_mt_rand_range(0, (zend_long) size - 1);

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

static zend_always_inline zend_bool php_parallel_events_poll_begin_link(
                                        php_parallel_events_t *events,
                                        php_parallel_events_state_t *state,
                                        zend_string *name,
                                        zend_object *object) {
    php_parallel_channel_t *channel =
        php_parallel_channel_fetch(object);

    php_parallel_link_lock(channel->link);

    if (php_parallel_link_closed(channel->link)) {
        state->closed = 1;
    } else {
        if (php_parallel_events_input_exists(&events->input, name)) {
            state->writable = php_parallel_link_writable(channel->link);
        } else {
            state->readable = php_parallel_link_readable(channel->link);
        }
    }

    if (state->readable || state->writable || state->closed) {
        state->type     = PHP_PARALLEL_EVENTS_LINK;
        state->name     = name;
        state->object   = object;

        return 1;
    }

    php_parallel_link_unlock(channel->link);
    return 0;
}

static zend_always_inline zend_bool php_parallel_events_poll_begin_future(
                            php_parallel_events_t *events,
                            php_parallel_events_state_t *state,
                            zend_string *name,
                            zend_object *object) {
    php_parallel_future_t *future =
        php_parallel_future_fetch(object);

    php_parallel_future_lock(future);

    state->readable = php_parallel_future_readable(future);

    if (state->readable) {
        state->type     = PHP_PARALLEL_EVENTS_FUTURE;
        state->name     = name;
        state->object   = object;

        return 1;
    }

    php_parallel_future_unlock(future);
    return 0;
}

static zend_always_inline zend_bool php_parallel_events_poll_begin(php_parallel_events_t *events, php_parallel_events_state_t *state) {
    zend_string *name;
    zend_object *object;

    if (!php_parallel_events_poll_random(events, &name, &object)) {
        return 0;
    }

    memset(state, 0, sizeof(php_parallel_events_state_t));

    if (instanceof_function(object->ce, php_parallel_channel_ce)) {
        return php_parallel_events_poll_begin_link(events, state, name, object);
    } else {
        return php_parallel_events_poll_begin_future(events, state, name, object);
    }

    return 0;
}

static zend_always_inline void php_parallel_events_poll_end(php_parallel_events_state_t *state) {
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

    if (state->closed) {
        php_parallel_events_event_construct(
            events,
            PHP_PARALLEL_EVENTS_EVENT_CLOSE,
            state->name,
            state->object,
            NULL,
            retval);

        return 1;
    } else {
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
    }

    return 0;
}

static zend_always_inline zend_bool php_parallel_events_poll_future(
                            php_parallel_events_t *events,
                            php_parallel_events_state_t *state,
                            zval *retval) {
    if (state->readable) {
        zval read;
        php_parallel_events_event_type_t type = PHP_PARALLEL_EVENTS_EVENT_READ;
        php_parallel_future_t *future =
            php_parallel_future_fetch(state->object);

        ZVAL_NULL(&read);

        if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_KILLED)) {
            type = PHP_PARALLEL_EVENTS_EVENT_KILL;
        } else if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_CANCELLED)) {
            type = PHP_PARALLEL_EVENTS_EVENT_CANCEL;
        } else {
            if (php_parallel_monitor_check(future->monitor, PHP_PARALLEL_ERROR)) {
                type = PHP_PARALLEL_EVENTS_EVENT_ERROR;
            }

            php_parallel_future_value(future, &read);
        }

        php_parallel_events_event_construct(
            events,
            type,
            state->name,
            state->object,
            &read,
            retval);

        return 1;
    }

    return 0;
}

void php_parallel_events_poll(php_parallel_events_t *events, zval *retval) {
    php_parallel_events_poll_t  poll;
    php_parallel_events_state_t selected;
    uint32_t        try = 1;

    if (!php_parallel_events_poll_init(&poll, events)) {
_php_parallel_events_poll_null:
        ZVAL_NULL(retval);
        return;
    }

    do {
        if (!php_parallel_events_poll_begin(events, &selected)) {
            if (!events->blocking) {
                goto _php_parallel_events_poll_null;
            }

            if (php_parallel_events_poll_timeout(&poll, events)) {
                return;
            }

            if ((try++ % 10) == 0) {
                usleep(1);
            }

            continue;
        }

        if (selected.type == PHP_PARALLEL_EVENTS_LINK) {
            if (php_parallel_events_poll_link(
                    events,
                    &selected,
                    retval)) {
                break;
            }
        } else {
            if (php_parallel_events_poll_future(
                    events,
                    &selected,
                    retval)) {
                break;
            }
        }

        php_parallel_events_poll_end(&selected);
    } while (1);

    php_parallel_events_poll_end(&selected);
}
#endif
