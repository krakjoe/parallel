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
#ifndef HAVE_PARALLEL_CACHE_H
#define HAVE_PARALLEL_CACHE_H

/*
* parallel is intended to be used with opcache, and whether it is loaded or
* not, this function shall return a permanently allocated (immutable)
* copy of the source function, with no reference count and immutable statics.
*/
zend_function* php_parallel_cache_function(const zend_function *source);

/*
* parallel must be able to buffer closures in persistent (but not permanent)
* memory for transfer to different contexts.
*
* this function shall return an (immutable) copy of the source closure,
* with no reference count, and immutable statics as they are
* at the time of the call.
* If destination is null it will be allocated for return value.
*/
zend_function* php_parallel_cache_closure(const zend_function *source, zend_function *destination);

PHP_MINIT_FUNCTION(PARALLEL_CACHE);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_CACHE);
#endif
