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
#ifndef PHP_PARALLEL_ARENA_H
# define PHP_PARALLEL_ARENA_H

typedef struct _php_parallel_arena_t php_parallel_arena_t;

php_parallel_arena_t* php_parallel_arena_create(zend_long size);
void* php_parallel_arena_alloc(php_parallel_arena_t *arena, zend_long size);
void php_parallel_arena_free(php_parallel_arena_t *arena, void *mem);
void php_parallel_arena_destroy(php_parallel_arena_t *arena);
#endif
