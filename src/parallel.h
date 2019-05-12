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
#ifndef HAVE_PARALLEL_PARALLEL_H
#define HAVE_PARALLEL_PARALLEL_H

#include "php.h"
#include "monitor.h"
#include "pthread.h"

#include "exceptions.h"
#include "handlers.h"
#include "runtime.h"
#include "future.h"
#include "scheduler.h"
#include "channel.h"
#include "events.h"
#include "event.h"
#include "input.h"

#include "SAPI.h"
#include "php_main.h"
#include "zend_closures.h"
#include "zend_gc.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "zend_vm.h"

#include "dependencies.h"
#include "cache.h"
#include "copy.h"
#include "check.h"

#define PARALLEL_PARAMETERS_NONE(r) \
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 0, 0) \
    ZEND_PARSE_PARAMETERS_END()

PHP_MINIT_FUNCTION(PARALLEL_CORE);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_CORE);

PHP_RINIT_FUNCTION(PARALLEL_CORE);
PHP_RSHUTDOWN_FUNCTION(PARALLEL_CORE);
#endif
