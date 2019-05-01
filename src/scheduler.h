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
#ifndef HAVE_PARALLEL_SCHEDULER_H
#define HAVE_PARALLEL_SCHEDULER_H

typedef struct _php_parallel_schedule_el_t {
    zend_execute_data          *frame;
} php_parallel_schedule_el_t;

void php_parallel_scheduler_startup(void);
void php_parallel_scheduler_shutdown(void);

void               php_parallel_scheduler_init(php_parallel_runtime_t *runtime);
void               php_parallel_scheduler_push(php_parallel_runtime_t *runtime, php_parallel_future_t *future, zend_function *function, zval *argv);
void               php_parallel_scheduler_destroy(php_parallel_runtime_t *runtime);

php_parallel_runtime_t* php_parallel_scheduler_setup(php_parallel_runtime_t *runtime);
zend_bool       php_parallel_scheduler_empty(php_parallel_runtime_t *runtime);
zend_bool       php_parallel_scheduler_pop(php_parallel_runtime_t *runtime, php_parallel_schedule_el_t *el);
void            php_parallel_scheduler_run(php_parallel_runtime_t *runtime, zend_execute_data *frame);
void            php_parallel_scheduler_kill(php_parallel_runtime_t *runtime);
void            php_parallel_scheduler_exit(php_parallel_runtime_t *runtime);

void            php_parallel_schedule_kill(php_parallel_runtime_t *runtime);

zend_bool       php_parallel_scheduler_cancel(php_parallel_future_t *future);
#endif
