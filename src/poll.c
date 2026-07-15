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

#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define PHP_PARALLEL_EVENTS_NANO_IN_SEC 1000000000ULL

#if PHP_VERSION_ID >= 80400
#include "ext/random/php_random.h"
#else
#include "ext/standard/php_mt_rand.h"
#endif

typedef struct _php_parallel_events_poll_notifier_t {
	pthread_mutex_t mutex;
	pthread_cond_t  condition;
	uint64_t        epoch;
} php_parallel_events_poll_notifier_t;

typedef struct _php_parallel_events_poll_t {
	uint64_t epoch;
	uint64_t stop;
	struct {
		zend_fcall_info       fci;
		zend_fcall_info_cache fcc;
		zval                  ival;
	} block;
	php_parallel_events_state_t state;
} php_parallel_events_poll_t;

static php_parallel_events_poll_notifier_t php_parallel_events_poll_notifier;

int                                        php_parallel_events_poll_startup(void)
{
	if (!php_parallel_mutex_init(&php_parallel_events_poll_notifier.mutex, 0)) {
		return FAILURE;
	}

	if (!php_parallel_cond_init(&php_parallel_events_poll_notifier.condition)) {
		php_parallel_mutex_destroy(&php_parallel_events_poll_notifier.mutex);
		return FAILURE;
	}

	php_parallel_events_poll_notifier.epoch = 0;

	return SUCCESS;
}

void php_parallel_events_poll_shutdown(void)
{
	php_parallel_cond_destroy(&php_parallel_events_poll_notifier.condition);
	php_parallel_mutex_destroy(&php_parallel_events_poll_notifier.mutex);
}

void php_parallel_events_poll_notify(void)
{
	pthread_mutex_lock(&php_parallel_events_poll_notifier.mutex);
	php_parallel_events_poll_notifier.epoch++;
	pthread_cond_broadcast(&php_parallel_events_poll_notifier.condition);
	pthread_mutex_unlock(&php_parallel_events_poll_notifier.mutex);
}

static zend_always_inline uint64_t php_parallel_events_poll_epoch(void)
{
	uint64_t epoch;

	pthread_mutex_lock(&php_parallel_events_poll_notifier.mutex);
	epoch = php_parallel_events_poll_notifier.epoch;
	pthread_mutex_unlock(&php_parallel_events_poll_notifier.mutex);

	return epoch;
}

static zend_always_inline uint64_t php_parallel_events_poll_now(void)
{
#ifdef _WIN32
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);

	return (now.QuadPart / frequency.QuadPart) * PHP_PARALLEL_EVENTS_NANO_IN_SEC +
	       (now.QuadPart % frequency.QuadPart) * PHP_PARALLEL_EVENTS_NANO_IN_SEC / frequency.QuadPart;
#else
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	return (uint64_t)now.tv_sec * PHP_PARALLEL_EVENTS_NANO_IN_SEC + now.tv_nsec;
#endif
}

static zend_always_inline php_parallel_events_poll_t *php_parallel_events_poll_init(php_parallel_events_t *events)
{
	php_parallel_events_poll_t *poll;

	if (events->targets.nNumUsed == 0) {
		return NULL;
	}

	poll = (php_parallel_events_poll_t *)pecalloc(1, sizeof(php_parallel_events_poll_t), 1);
	poll->epoch = php_parallel_events_poll_epoch();

	if (events->timeout > -1) {
		uint64_t now = php_parallel_events_poll_now();
		uint64_t duration;

		if ((zend_ulong)events->timeout > UINT64_MAX / 1000) {
			duration = UINT64_MAX;
		} else {
			duration = (uint64_t)events->timeout * 1000;
		}

		poll->stop = duration > UINT64_MAX - now ? UINT64_MAX : now + duration;
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

static zend_always_inline void php_parallel_events_poll_realtime(struct timespec *timeout, uint64_t remaining)
{
	timespec_get(timeout, TIME_UTC);

	timeout->tv_sec += remaining / PHP_PARALLEL_EVENTS_NANO_IN_SEC;
	timeout->tv_nsec += remaining % PHP_PARALLEL_EVENTS_NANO_IN_SEC;

	if (timeout->tv_nsec >= PHP_PARALLEL_EVENTS_NANO_IN_SEC) {
		timeout->tv_sec++;
		timeout->tv_nsec -= PHP_PARALLEL_EVENTS_NANO_IN_SEC;
	}
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

static zend_always_inline bool php_parallel_events_poll_wait(php_parallel_events_poll_t *poll,
                                                             php_parallel_events_t      *events)
{
	uint64_t epoch;

	pthread_mutex_lock(&php_parallel_events_poll_notifier.mutex);

	while (php_parallel_events_poll_notifier.epoch == poll->epoch) {
		int result;

		if (events->timeout > -1) {
			uint64_t        now = php_parallel_events_poll_now();
			struct timespec timeout;

			if (now >= poll->stop) {
				break;
			}

			php_parallel_events_poll_realtime(&timeout, poll->stop - now);
			result = pthread_cond_timedwait(&php_parallel_events_poll_notifier.condition,
			                                &php_parallel_events_poll_notifier.mutex, &timeout);

			if (result != SUCCESS && result != ETIMEDOUT) {
				break;
			}
		} else {
			result = pthread_cond_wait(&php_parallel_events_poll_notifier.condition,
			                           &php_parallel_events_poll_notifier.mutex);

			if (result != SUCCESS) {
				break;
			}
		}
	}

	epoch = php_parallel_events_poll_notifier.epoch;
	pthread_mutex_unlock(&php_parallel_events_poll_notifier.mutex);

	if (epoch != poll->epoch) {
		poll->epoch = epoch;
		return 1;
	}

	return !php_parallel_events_poll_expired(poll, events);
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

				if (php_parallel_link_send(channel->link, input)) {

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
