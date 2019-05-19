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
#ifndef HAVE_PARALLEL_CHANNEL_H
#define HAVE_PARALLEL_CHANNEL_H

#include "link.h"

typedef struct _php_parallel_channel_t {
    php_parallel_link_t *link;
    zend_object std;
} php_parallel_channel_t;

extern zend_class_entry *php_parallel_channel_ce;
extern zend_object_handlers php_parallel_channel_handlers;

static zend_always_inline php_parallel_channel_t* php_parallel_channel_fetch(zend_object *o) {
    return (php_parallel_channel_t*) (((char*) o) - XtOffsetOf(php_parallel_channel_t, std));
}

static zend_always_inline php_parallel_channel_t* php_parallel_channel_from(zval *z) {
    return php_parallel_channel_fetch(Z_OBJ_P(z));
}

PHP_MINIT_FUNCTION(PARALLEL_CHANNEL);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_CHANNEL);
#endif
