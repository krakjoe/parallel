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
#ifndef HAVE_PARALLEL_LIST_H
#define HAVE_PARALLEL_LIST_H

typedef struct _php_parallel_list_t php_parallel_list_t;

php_parallel_list_t* php_parallel_list_create(zend_ulong size, llist_dtor_func_t dtor);
void                 php_parallel_list_apply(php_parallel_list_t *list, llist_apply_func_t func);
void*                php_parallel_list_append(php_parallel_list_t *list, void *mem);
void                 php_parallel_list_delete(php_parallel_list_t *list, void *mem);
zend_ulong           php_parallel_list_count(php_parallel_list_t *list);
void*                php_parallel_list_head(php_parallel_list_t *list);
void*                php_parallel_list_tail(php_parallel_list_t *list);
void                 php_parallel_list_destroy(php_parallel_list_t *list);
void                 php_parallel_list_free(php_parallel_list_t *list);
#endif
