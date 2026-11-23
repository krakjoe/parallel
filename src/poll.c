/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2024                                  |
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

#if PHP_VERSION_ID >= 80300
#include "Zend/zend_hrtime.h"
#endif

#include <errno.h>
#include <time.h>

#ifdef _WIN32
#if PHP_VERSION_ID < 80300
#include <windows.h>
#endif
#else
#include <sys/select.h>
#endif

#define PHP_PARALLEL_EVENTS_MICRO_IN_SEC 1000000ULL

#if PHP_VERSION_ID >= 80400
#include "ext/random/php_random.h"
#else
#include "ext/standard/php_mt_rand.h"
#endif

typedef struct _php_parallel_events_poll_t {
	uint32_t try;
	uint64_t stop;
	bool     native;
	struct {
		zend_fcall_info       fci;
		zend_fcall_info_cache fcc;
		zval                  ival;
	} block;
	php_parallel_events_state_t state;
} php_parallel_events_poll_t;

static zend_always_inline uint64_t php_parallel_events_poll_now(void)
{
#if PHP_VERSION_ID >= 80300
	return zend_hrtime() / 1000;
#elif defined(_WIN32)
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);

	return (now.QuadPart / frequency.QuadPart) * PHP_PARALLEL_EVENTS_MICRO_IN_SEC +
	       (now.QuadPart % frequency.QuadPart) * PHP_PARALLEL_EVENTS_MICRO_IN_SEC / frequency.QuadPart;
#else
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	return (uint64_t)now.tv_sec * PHP_PARALLEL_EVENTS_MICRO_IN_SEC + now.tv_nsec / 1000;
#endif
}

static zend_always_inline php_parallel_events_poll_t *php_parallel_events_poll_init(php_parallel_events_t *events)
{
	php_parallel_events_poll_t *poll;

	if (zend_hash_num_elements(&events->targets) == 0) {
		return NULL;
	}

	poll = (php_parallel_events_poll_t *)pecalloc(1, sizeof(php_parallel_events_poll_t), 1);
#ifndef _WIN32
	poll->native = true;
#endif

	if (events->timeout > -1) {
		poll->stop = php_parallel_events_poll_now() + (uint64_t)events->timeout;
	}

	if (!Z_ISUNDEF(events->blocker)) {
		zend_fcall_info_init(&events->blocker, 0, &poll->block.fci, &poll->block.fcc, NULL, NULL);
		poll->block.fci.retval = &poll->block.ival;
	} else {
		memset(&poll->block, 0, sizeof(poll->block));
	}

	return poll;
}

static zend_always_inline void php_parallel_events_poll_free(php_parallel_events_poll_t *poll) { pefree(poll, 1); }

static zend_always_inline void php_parallel_events_poll_unlock(php_parallel_events_poll_t *poll)
{
	if (poll->state.type == PHP_PARALLEL_EVENTS_LINK) {
		php_parallel_channel_t *channel = php_parallel_channel_fetch(poll->state.object);

		php_parallel_link_unlock(channel->link);
	} else {
		php_parallel_future_t *future = php_parallel_future_fetch(poll->state.object);

		php_parallel_future_unlock(future);
	}
}

static zend_always_inline void php_parallel_events_poll_end(php_parallel_events_poll_t *poll)
{
	php_parallel_events_poll_unlock(poll);
	php_parallel_events_poll_free(poll);
}

static zend_always_inline bool php_parallel_events_poll_expired(php_parallel_events_poll_t *poll,
                                                                php_parallel_events_t      *events)
{
	if (events->timeout > -1 && php_parallel_events_poll_now() >= poll->stop) {
		php_parallel_exception_ex(php_parallel_events_error_timeout_ce, "timeout occured");
		return 1;
	}

	return 0;
}

static zend_always_inline bool php_parallel_events_poll_busy(php_parallel_events_poll_t *poll,
                                                             php_parallel_events_t      *events)
{
	if ((poll->try++ % 10) == 0) {
		usleep(1);
	}

	return !php_parallel_events_poll_expired(poll, events);
}

#ifndef _WIN32
static zend_always_inline bool php_parallel_events_poll_descriptors(php_parallel_events_t *events, fd_set *readfds,
                                                                    int *maximum)
{
	uint32_t index;

	FD_ZERO(readfds);
	*maximum = -1;

	for (index = 0; index < events->targets.nNumUsed; index++) {
		Bucket      *bucket = &events->targets.arData[index];
		zend_object *object;
		int          descriptor;

		if (Z_ISUNDEF(bucket->val)) {
			continue;
		}

		object = Z_OBJ(bucket->val);

		if (instanceof_function(object->ce, php_parallel_channel_ce)) {
			php_parallel_channel_t *channel = php_parallel_channel_fetch(object);
			bool                    writable = php_parallel_events_input_exists(&events->input, bucket->key);

			php_parallel_link_lock(channel->link);
			descriptor = php_parallel_link_notify(channel->link, writable);
			php_parallel_link_unlock(channel->link);
		} else {
			php_parallel_future_t *future = php_parallel_future_fetch(object);

			php_parallel_future_lock(future);
			descriptor = php_parallel_monitor_notify(future->monitor);
			php_parallel_future_unlock(future);
		}

		if (descriptor < 0 || descriptor >= FD_SETSIZE) {
			return false;
		}

		FD_SET(descriptor, readfds);
		if (descriptor > *maximum) {
			*maximum = descriptor;
		}
	}

	return *maximum >= 0;
}

static zend_always_inline bool php_parallel_events_poll_native(php_parallel_events_poll_t *poll,
                                                               php_parallel_events_t      *events)
{
	fd_set          readfds;
	int             maximum;
	int             result;
	struct timeval  timeout;
	struct timeval *timeout_pointer = NULL;

	if (!php_parallel_events_poll_descriptors(events, &readfds, &maximum)) {
		poll->native = false;
		return php_parallel_events_poll_busy(poll, events);
	}

	if (events->timeout > -1) {
		uint64_t now = php_parallel_events_poll_now();
		uint64_t remaining;

		if (now >= poll->stop) {
			return !php_parallel_events_poll_expired(poll, events);
		}

		remaining = poll->stop - now;
		timeout.tv_sec = (long)(remaining / PHP_PARALLEL_EVENTS_MICRO_IN_SEC);
		timeout.tv_usec = (long)(remaining % PHP_PARALLEL_EVENTS_MICRO_IN_SEC);
		timeout_pointer = &timeout;
	}

	result = select(maximum + 1, &readfds, NULL, NULL, timeout_pointer);

	if (result >= 0) {
		return result > 0 || !php_parallel_events_poll_expired(poll, events);
	}

	if (errno == EINTR) {
		return true;
	}

	poll->native = false;
	return php_parallel_events_poll_busy(poll, events);
}
#endif

static zend_always_inline bool php_parallel_events_poll_wait(php_parallel_events_poll_t *poll,
                                                             php_parallel_events_t      *events)
{
#ifndef _WIN32
	if (poll->native) {
		return php_parallel_events_poll_native(poll, events);
	}
#endif

	return php_parallel_events_poll_busy(poll, events);
}

static zend_always_inline bool php_parallel_events_poll_begin_link(php_parallel_events_t       *events,
                                                                   php_parallel_events_state_t *state,
                                                                   zend_string *name, zend_object *object)
{
	php_parallel_channel_t *channel = php_parallel_channel_fetch(object);

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
		state->type = PHP_PARALLEL_EVENTS_LINK;
		state->name = name;
		state->object = object;

		return 1;
	}

	php_parallel_link_unlock(channel->link);
	return 0;
}

static zend_always_inline bool php_parallel_events_poll_begin_future(php_parallel_events_t       *events,
                                                                     php_parallel_events_state_t *state,
                                                                     zend_string *name, zend_object *object)
{
	php_parallel_future_t *future = php_parallel_future_fetch(object);

	php_parallel_future_lock(future);

	state->readable = php_parallel_future_readable(future);

	if (state->readable) {
		state->type = PHP_PARALLEL_EVENTS_FUTURE;
		state->name = name;
		state->object = object;

		return 1;
	}

	php_parallel_future_unlock(future);
	return 0;
}

static zend_always_inline bool php_parallel_events_poll_begin(php_parallel_events_t       *events,
                                                              php_parallel_events_state_t *state)
{
	uint32_t size = events->targets.nNumUsed;
	uint32_t index = (uint32_t)php_mt_rand_range(0, (zend_long)size - 1);
	uint32_t scanned;

	for (scanned = 0; scanned < size; scanned++) {
		Bucket      *bucket = &events->targets.arData[index];
		zend_object *object;

		if (++index == size) {
			index = 0;
		}

		if (Z_ISUNDEF(bucket->val)) {
			continue;
		}

		memset(state, 0, sizeof(php_parallel_events_state_t));
		object = Z_OBJ(bucket->val);

		if (instanceof_function(object->ce, php_parallel_channel_ce)) {
			if (php_parallel_events_poll_begin_link(events, state, bucket->key, object)) {
				return 1;
			}
		} else if (php_parallel_events_poll_begin_future(events, state, bucket->key, object)) {
			return 1;
		}
	}

	return 0;
}

static zend_always_inline bool php_parallel_events_poll_link(php_parallel_events_t       *events,
                                                             php_parallel_events_state_t *state, zval *retval)
{
	php_parallel_channel_t *channel = php_parallel_channel_fetch(state->object);
	zval                   *input;

	if (state->closed) {
		php_parallel_events_event_construct(events, PHP_PARALLEL_EVENTS_EVENT_CLOSE, state->name, state->object, NULL,
		                                    retval);

		return 1;
	} else {
		if ((input = php_parallel_events_input_find(&events->input, state->name))) {

			if (state->writable) {

				if (php_parallel_link_send_event(channel->link, input)) {

					php_parallel_events_event_construct(events, PHP_PARALLEL_EVENTS_EVENT_WRITE, state->name,
					                                    state->object, NULL, retval);

					return 1;
				}
			}
		} else {
			if (state->readable) {
				zval read;

				if (php_parallel_link_recv(channel->link, &read)) {

					php_parallel_events_event_construct(events, PHP_PARALLEL_EVENTS_EVENT_READ, state->name,
					                                    state->object, &read, retval);

					return 1;
				}
			}
		}
	}

	return 0;
}

static zend_always_inline bool php_parallel_events_poll_future(php_parallel_events_t       *events,
                                                               php_parallel_events_state_t *state, zval *retval)
{
	if (state->readable) {
		zval                             read;
		php_parallel_events_event_type_t type = PHP_PARALLEL_EVENTS_EVENT_READ;
		php_parallel_future_t           *future = php_parallel_future_fetch(state->object);

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

		php_parallel_events_event_construct(events, type, state->name, state->object, &read, retval);

		return 1;
	}

	return 0;
}

void php_parallel_events_poll(php_parallel_events_t *events, zval *retval)
{
	php_parallel_events_poll_t *poll;

	if (!(poll = php_parallel_events_poll_init(events))) {
		ZVAL_NULL(retval);
		return;
	}

	do {
		if (!php_parallel_events_poll_begin(events, &poll->state)) {
			if (!events->blocking) {
				php_parallel_events_poll_free(poll);
				return;
			}

			if (poll->block.fci.size) {
				zend_call_function(&poll->block.fci, &poll->block.fcc);

				if (zend_is_true(&poll->block.ival)) {
					zval_ptr_dtor(&poll->block.ival);
					php_parallel_events_poll_free(poll);
					return;
				}

				zval_ptr_dtor(&poll->block.ival);

				if (php_parallel_events_poll_expired(poll, events)) {
					php_parallel_events_poll_free(poll);
					return;
				}

				continue;
			}

			if (!php_parallel_events_poll_wait(poll, events)) {
				php_parallel_events_poll_free(poll);
				return;
			}

			continue;
		}

		if (poll->state.type == PHP_PARALLEL_EVENTS_LINK) {
			if (php_parallel_events_poll_link(events, &poll->state, retval)) {
				break;
			}
		} else {
			if (php_parallel_events_poll_future(events, &poll->state, retval)) {
				break;
			}
		}

		/* Unlock but don't free - we're continuing the loop */
		php_parallel_events_poll_unlock(poll);
	} while (1);

	php_parallel_events_poll_end(poll);
}
#endif
