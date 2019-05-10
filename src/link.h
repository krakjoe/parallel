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
#ifndef HAVE_PARALLEL_LINK_H
#define HAVE_PARALLEL_LINK_H

typedef struct _php_parallel_link_t php_parallel_link_t;

php_parallel_link_t* php_parallel_link_init(zend_string *name, zend_bool buffered, zend_long capacity);
zend_string*         php_parallel_link_name(php_parallel_link_t *link);
php_parallel_link_t* php_parallel_link_copy(php_parallel_link_t *link);
zend_bool            php_parallel_link_send(php_parallel_link_t *link, zval *value);
zend_bool            php_parallel_link_recv(php_parallel_link_t *link, zval *value);
zend_bool            php_parallel_link_close(php_parallel_link_t *link);
zend_bool            php_parallel_link_closed(php_parallel_link_t *link);
void                 php_parallel_link_destroy(php_parallel_link_t *link);

zend_bool            php_parallel_link_lock(php_parallel_link_t *link);
zend_bool            php_parallel_link_writable(php_parallel_link_t *link);
zend_bool            php_parallel_link_readable(php_parallel_link_t *link);
zend_bool            php_parallel_link_unlock(php_parallel_link_t *link);

void                 php_parallel_link_debug(php_parallel_link_t *link, HashTable *debug);

PHP_MINIT_FUNCTION(PARALLEL_LINK);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_LINK);
#endif
