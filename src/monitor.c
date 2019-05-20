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

#include "parallel.h"

php_parallel_monitor_t* php_parallel_monitor_create(void) {
    php_parallel_monitor_t *monitor =
        (php_parallel_monitor_t*)
            calloc(1, sizeof(php_parallel_monitor_t));

    php_parallel_mutex_init(&monitor->mutex, 1);
    php_parallel_cond_init(&monitor->condition);

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

void php_parallel_monitor_set(php_parallel_monitor_t *monitor, int32_t state) {
    pthread_mutex_lock(&monitor->mutex);

    monitor->state |= state;

    pthread_cond_signal(
        &monitor->condition);

    pthread_mutex_unlock(&monitor->mutex);
}

void php_parallel_monitor_add(php_parallel_monitor_t *monitor, int32_t state) {
    pthread_mutex_lock(&monitor->mutex);

    monitor->state |= state;

    pthread_mutex_unlock(&monitor->mutex);
}

void php_parallel_monitor_remove(php_parallel_monitor_t *monitor, int32_t state) {
    pthread_mutex_lock(&monitor->mutex);

    monitor->state &= ~state;

    pthread_mutex_unlock(&monitor->mutex);
}

void php_parallel_monitor_destroy(php_parallel_monitor_t *monitor) {
    php_parallel_mutex_destroy(&monitor->mutex);
    php_parallel_cond_destroy(&monitor->condition);

    free(monitor);
}
#endif
