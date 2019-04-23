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
#ifndef HAVE_PARALLEL_MONITOR
#define HAVE_PARALLEL_MONITOR

#include "php.h"
#include "monitor.h"

php_parallel_monitor_t* php_parallel_monitor_create(void) {
	pthread_mutexattr_t attributes;
	php_parallel_monitor_t *monitor = 
		(php_parallel_monitor_t*) 
			calloc(1, sizeof(php_parallel_monitor_t));

	pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

	pthread_mutex_init(&monitor->mutex, &attributes);
	pthread_mutexattr_destroy(&attributes);

	pthread_cond_init(&monitor->condition, NULL);

	return monitor;
}

int php_parallel_monitor_lock(php_parallel_monitor_t *monitor) {
	return pthread_mutex_lock(&monitor->mutex);
}

int32_t php_parallel_monitor_check(php_parallel_monitor_t *monitor, int32_t state) {
	return (monitor->state & (state));
}

int php_parallel_monitor_unlock(php_parallel_monitor_t *monitor) {
	return pthread_mutex_unlock(&monitor->mutex);
}

int32_t php_parallel_monitor_wait(php_parallel_monitor_t *monitor, int32_t state) {
	int32_t changed = FAILURE;
	int      rc      = SUCCESS;

	if (pthread_mutex_lock(&monitor->mutex) != SUCCESS) {
		return FAILURE;
	}

	while (!(changed = (monitor->state & state))) {

		if ((rc = pthread_cond_wait(
				&monitor->condition, &monitor->mutex)) != SUCCESS) {
			pthread_mutex_unlock(&monitor->mutex);

			return FAILURE;
		}
	}

	monitor->state ^= changed;

	if (pthread_mutex_unlock(&monitor->mutex) != SUCCESS) {
		return FAILURE;
	}

	return changed;
}

int32_t php_parallel_monitor_wait_locked(php_parallel_monitor_t *monitor, int32_t state) {
	int32_t changed = FAILURE;
	int      rc      = SUCCESS;

	while (!(changed = (monitor->state & state))) {
		if ((rc = pthread_cond_wait(
				&monitor->condition, &monitor->mutex)) != SUCCESS) {
			return FAILURE;
		}
	}

	monitor->state ^= changed;

	return changed;
}

void php_parallel_monitor_set(php_parallel_monitor_t *monitor, int32_t state, zend_bool lock) {
	if (lock) {
		pthread_mutex_lock(&monitor->mutex);
	}

	monitor->state |= state;

	pthread_cond_signal(&monitor->condition);

	if (lock) {
		pthread_mutex_unlock(&monitor->mutex);
	}
}

void php_parallel_monitor_destroy(php_parallel_monitor_t *monitor) {
	pthread_mutex_destroy(&monitor->mutex);
	pthread_cond_destroy(&monitor->condition);

	free(monitor);
}
#endif
