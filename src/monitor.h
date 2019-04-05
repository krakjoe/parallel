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

#define PHP_PARALLEL_READY  0x00000001
#define PHP_PARALLEL_EXEC   0x00000010
#define PHP_PARALLEL_CLOSE  0x00001000
#define PHP_PARALLEL_KILLED 0x00010000
#define PHP_PARALLEL_DONE   0x00100000
#define PHP_PARALLEL_CLOSED 0x01000000 
#define PHP_PARALLEL_ERROR  0x10000000

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
