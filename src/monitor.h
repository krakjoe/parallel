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
#ifndef HAVE_PARALLEL_MONITOR_H
#define HAVE_PARALLEL_MONITOR_H

#include <pthread.h>

typedef struct _php_parallel_monitor_t {
	pthread_mutex_t  mutex;
	pthread_cond_t   condition;
	volatile int32_t state;
} php_parallel_monitor_t;

#define PHP_PARALLEL_READY     (1<<0)
#define PHP_PARALLEL_EXEC      (1<<1)
#define PHP_PARALLEL_CLOSE     (1<<2)
#define PHP_PARALLEL_CLOSED    (1<<3)
#define PHP_PARALLEL_KILLED    (1<<4)
#define PHP_PARALLEL_ERROR     (1<<5)

#define PHP_PARALLEL_YIELDING  (1<<6)

#define PHP_PARALLEL_DONE      (1<<10)

static inline unsigned char php_parallel_monitor_wipe(php_parallel_monitor_t *monitor, int32_t state) {
    unsigned char wiped = (monitor->state & state) == state;
    
    monitor->state &= ~state;
    
    return wiped;
}

php_parallel_monitor_t* php_parallel_monitor_create(void);
int php_parallel_monitor_lock(php_parallel_monitor_t *m);
int32_t php_parallel_monitor_check(php_parallel_monitor_t *m, int32_t state);
int php_parallel_monitor_unlock(php_parallel_monitor_t *m);
int32_t php_parallel_monitor_wait(php_parallel_monitor_t *m, int32_t state);
int32_t php_parallel_monitor_wait_timed(php_parallel_monitor_t *monitor, int32_t state, zend_long timeout);
int32_t php_parallel_monitor_wait_locked(php_parallel_monitor_t *m, int32_t state);
void php_parallel_monitor_set(php_parallel_monitor_t *monitor, int32_t state, zend_bool lock);
void php_parallel_monitor_destroy(php_parallel_monitor_t *);
#endif
