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
#ifndef HAVE_PARALLEL_GROUP_RESULT_H
#define HAVE_PARALLEL_GROUP_RESULT_H

#include "group.h"

typedef enum {
    PHP_PARALLEL_GROUP_RESULT_READ = 1,
    PHP_PARALLEL_GROUP_RESULT_WRITE
} php_parallel_group_result_type_t;

void php_parallel_group_result_startup(void);
void php_parallel_group_result_shutdown(void);

void php_parallel_group_result_construct(
        php_parallel_group_t *group,
        php_parallel_group_result_type_t type, 
        zend_string *source,
        zend_object *object,
        zval *value,
        zval *return_value);

#endif
