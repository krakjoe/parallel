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
#ifndef HAVE_PARALLEL_EVENTS_EVENT_H
#define HAVE_PARALLEL_EVENTS_EVENT_H

#include "events.h"

typedef enum {
    PHP_PARALLEL_EVENTS_EVENT_READ = 1,
    PHP_PARALLEL_EVENTS_EVENT_WRITE,
    PHP_PARALLEL_EVENTS_EVENT_CLOSE,
    PHP_PARALLEL_EVENTS_EVENT_ERROR,
    PHP_PARALLEL_EVENTS_EVENT_CANCEL,
    PHP_PARALLEL_EVENTS_EVENT_KILL
} php_parallel_events_event_type_t;

PHP_MINIT_FUNCTION(PARALLEL_EVENTS_EVENT);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS_EVENT);

void php_parallel_events_event_construct(
        php_parallel_events_t *events,
        php_parallel_events_event_type_t type,
        zend_string *source,
        zend_object *object,
        zval *value,
        zval *return_value);

zend_string* php_parallel_events_event_source_local(zend_string *source);
#endif
