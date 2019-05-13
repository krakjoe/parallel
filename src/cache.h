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
* parallel must buffer closures and functions in order to transfer them to
* other threads.
*
* parallel is intended to be used with opcache, and whether it is loaded or
* not, this function shall return a persistently allocated (immutable)
* copy of the source function, with no reference count and immutable statics.
*
* should source already be cached by opcache, each function shall consume:
*   sizeof(zend_function) +
*   sizeof(Bucket)
*
* should the function have statics another sizeof(Bucket) may be used in
* addition to the statics table (size depends on number of static vars)
*
* should source not be cached by opcache, parallel will cache it, overhead
* depends on source instructions, vars, etc.
*/

zend_function* php_parallel_cache_function(const zend_function *source);

PHP_MINIT_FUNCTION(PARALLEL_CACHE);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_CACHE);
#endif
