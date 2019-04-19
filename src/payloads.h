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
#ifndef HAVE_PARALLEL_EVENTS_PAYLOADS_H
#define HAVE_PARALLEL_EVENTS_PAYLOADS_H

extern zend_class_entry* php_parallel_events_payloads_ce;

void php_parallel_events_payloads_startup(void);
void php_parallel_events_payloads_shutdown(void);

zend_bool php_parallel_events_payloads_exists(zval *zv, zend_string *target);
zval*     php_parallel_events_payloads_find(zval *payloads, zend_string *target);
zend_bool php_parallel_events_payloads_remove(zval *payloads, zend_string *target);
#endif
