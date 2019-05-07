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
#ifndef HAVE_PARALLEL_EVENTS_INPUT_H
#define HAVE_PARALLEL_EVENTS_INPUT_H

extern zend_class_entry* php_parallel_events_input_ce;

zend_bool php_parallel_events_input_exists(zval *zv, zend_string *target);
zval*     php_parallel_events_input_find(zval *input, zend_string *target);
zend_bool php_parallel_events_input_remove(zval *input, zend_string *target);

PHP_MINIT_FUNCTION(PARALLEL_EVENTS_INPUT);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_EVENTS_INPUT);
#endif
