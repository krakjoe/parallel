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
	pthread_mutexattr_t at;
	php_parallel_monitor_t *monitor = 
		(php_parallel_monitor_t*) 
			calloc(1, sizeof(php_parallel_monitor_t));

	pthread_mutexattr_init(&at);
	pthread_mutex_init(&monitor->mutex, &at);
	pthread_mutexattr_destroy(&at);

	pthread_cond_init(&monitor->condition, NULL);

	return monitor;
}

int php_parallel_monitor_lock(php_parallel_monitor_t *monitor) {
	return pthread_mutex_lock(&monitor->mutex);
}

uint32_t php_parallel_monitor_check(php_parallel_monitor_t *monitor, uint32_t state) {
	return (monitor->state & (state));
}

int php_parallel_monitor_unlock(php_parallel_monitor_t *monitor) {
	return pthread_mutex_unlock(&monitor->mutex);
}

uint32_t php_parallel_monitor_wait(php_parallel_monitor_t *monitor, uint32_t state) {
	uint32_t changed = FAILURE;
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

		if (monitor->state & (PHP_PARALLEL_DONE|PHP_PARALLEL_CLOSE)) {
			changed = FAILURE;
			pthread_mutex_unlock(&monitor->mutex);

			return changed;
		}
	}

	monitor->state ^= changed;

	if (pthread_mutex_unlock(&monitor->mutex) != SUCCESS) {
		return FAILURE;
	}

	return changed;
}

uint32_t php_parallel_monitor_wait_locked(php_parallel_monitor_t *monitor, uint32_t state) {
	uint32_t changed = FAILURE;
	int      rc      = SUCCESS;

	while (!(changed = (monitor->state & state))) {

		if ((rc = pthread_cond_wait(
				&monitor->condition, &monitor->mutex)) != SUCCESS) {
			return FAILURE;
		}

		if (monitor->state & (PHP_PARALLEL_DONE|PHP_PARALLEL_CLOSE)) {
			return FAILURE;
		}
	}

	monitor->state ^= changed;

	return changed;
}

void php_parallel_monitor_set(php_parallel_monitor_t *monitor, uint32_t state) {
	monitor->state |= state;

	pthread_cond_signal(&monitor->condition);
}

void php_parallel_monitor_unset(php_parallel_monitor_t *monitor, uint32_t state) {
	monitor->state &= ~state;

	pthread_cond_signal(&monitor->condition);
}

void php_parallel_monitor_destroy(php_parallel_monitor_t *monitor) {
	pthread_mutex_destroy(&monitor->mutex);
	pthread_cond_destroy(&monitor->condition);

	free(monitor);
}
#endif
