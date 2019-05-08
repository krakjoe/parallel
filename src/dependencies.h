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
#ifndef HAVE_PARALLEL_DEPENDENCIES_H
#define HAVE_PARALLEL_DEPENDENCIES_H

void php_parallel_dependencies_store(const zend_function *function);
void php_parallel_dependencies_load(const zend_function *function);

PHP_MINIT_FUNCTION(PARALLEL_DEPENDENCIES);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_DEPENDENCIES);

PHP_RINIT_FUNCTION(PARALLEL_DEPENDENCIES);
PHP_RSHUTDOWN_FUNCTION(PARALLEL_DEPENDENCIES);
#endif
