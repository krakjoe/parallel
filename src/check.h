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
#ifndef HAVE_PARALLEL_CHECK_H
#define HAVE_PARALLEL_CHECK_H

zend_function* php_parallel_check_task(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns);

zend_bool      php_parallel_check_zval(zval *zv, zval **error);
zend_bool      php_parallel_check_resource(zval *zv);
zend_bool      php_parallel_check_function(const zend_function *function, zend_function **errf, zend_uchar *erro);

#define PARALLEL_ZVAL_CHECK php_parallel_check_zval

void php_parallel_check_startup(void);
void php_parallel_check_shutdown(void);
#endif
