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
#ifndef HAVE_PARALLEL_STRINGS
#define HAVE_PARALLEL_STRINGS

#include "parallel.h"

TSRM_TLS HashTable php_parallel_strings;

void php_parallel_strings_release(zval *zv) {
    zend_string_release(Z_PTR_P(zv));
}

void php_parallel_strings_startup() {
    zend_hash_init(
        &php_parallel_strings,
        32, NULL,
        php_parallel_strings_release, 0);
}

zend_string* php_parallel_string(zend_string *source) {
    zend_string *local = 
        (zend_string*)
            zend_hash_find_ptr(
                &php_parallel_strings, source);
            
    if (!local) {
        local = zend_string_dup(source, 0);
        
        zend_hash_add_ptr(
            &php_parallel_strings, 
            local, local);
    }
    
    return local;
}

void php_parallel_strings_shutdown() {
    zend_hash_destroy(&php_parallel_strings);
}
#endif
