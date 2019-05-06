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
#ifndef HAVE_PARALLEL_EVENTS_LOOP
#define HAVE_PARALLEL_EVENTS_LOOP

#include "parallel.h"
#include "poll.h"

typedef struct _php_parallel_events_loop_t {
    zend_object_iterator it;
    zval events;
    zval event;
} php_parallel_events_loop_t;

static void php_parallel_events_loop_destroy(zend_object_iterator *zo) {
    php_parallel_events_loop_t *loop =
        (php_parallel_events_loop_t*) zo;

    if (!Z_ISUNDEF(loop->event)) {
        zval_ptr_dtor(&loop->event);
    }

    zval_ptr_dtor(&loop->events);
}

static int php_parallel_events_loop_valid(zend_object_iterator *zo) {
    php_parallel_events_loop_t *loop =
        (php_parallel_events_loop_t*) zo;

    return Z_TYPE(loop->event) == IS_OBJECT ? SUCCESS : FAILURE;
}

static void php_parallel_events_loop_poll(zend_object_iterator *zo) {
    php_parallel_events_loop_t *loop =
        (php_parallel_events_loop_t*) zo;
    php_parallel_events_t *events =
        php_parallel_events_from(&loop->events);

    if (Z_TYPE(loop->event) == IS_OBJECT) {
        zval_ptr_dtor(&loop->event);
    }

    php_parallel_events_poll(events, &loop->event);
}

static zval* php_parallel_events_loop_current(zend_object_iterator *zo) {
    php_parallel_events_loop_t *loop =
        (php_parallel_events_loop_t*) zo;

    return &loop->event;
}

#if PHP_VERSION_ID >= 70300
const
#endif
zend_object_iterator_funcs php_parallel_events_loop_functions = {
    .dtor               = php_parallel_events_loop_destroy,
    .valid              = php_parallel_events_loop_valid,
    .move_forward       = php_parallel_events_loop_poll,
    .get_current_data   = php_parallel_events_loop_current,
    .rewind             = php_parallel_events_loop_poll,
    .invalidate_current = NULL,
    .get_current_key    = NULL,
};

static zend_always_inline zend_bool php_parallel_events_loop_check(zval *zv) {
    php_parallel_events_t *events = php_parallel_events_from(zv);

    if (events->blocking) {
        return 1;
    }

    return 0;
}

zend_object_iterator* php_parallel_events_loop_create(zend_class_entry *type, zval *events, int by_ref) {
    php_parallel_events_loop_t *loop;

    if (!php_parallel_events_loop_check(events)) {
        php_parallel_exception_ex(
            php_parallel_events_error_ce,
            "cannot create iterator for non-blocking event loop");
        return NULL;
    }

    loop =
        (php_parallel_events_loop_t*)
            ecalloc(1, sizeof(php_parallel_events_loop_t));

    zend_iterator_init(&loop->it);

    loop->it.funcs = &php_parallel_events_loop_functions;

    ZVAL_COPY(&loop->events, events);

    ZVAL_UNDEF(&loop->event);

    return &loop->it;
}
#endif
