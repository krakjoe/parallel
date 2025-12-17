/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2024                                  |
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
#ifndef HAVE_PARALLEL_HANDLERS
#define HAVE_PARALLEL_HANDLERS

#include "parallel.h"

zend_object_handlers _php_parallel_standard_handlers;

zval* php_parallel_handlers_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv) {
    php_parallel_exception(
        "%s objects do not support properties",
        ZSTR_VAL(object->ce->name));

    return &EG(uninitialized_zval);
}

zval* php_parallel_handlers_write_property(zend_object *object, zend_string *member, zval *value, void **cache_slot) {
    php_parallel_exception(
        "%s objects do not support properties",
        ZSTR_VAL(object->ce->name));

    return &EG(uninitialized_zval);
}

zval* php_parallel_handlers_read_dimension(zend_object *object, zval *offset, int type, zval *rv) {
    php_parallel_exception(
        "%s objects do not support dimensions",
        ZSTR_VAL(object->ce->name));

    return &EG(uninitialized_zval);
}

void php_parallel_handlers_write_dimension(zend_object *object, zval *offset, zval *value) {
    php_parallel_exception(
        "%s objects do not support dimensions",
        ZSTR_VAL(object->ce->name));
}

zval* php_parallel_handlers_get_property_ptr_ptr(zend_object *object, zend_string *member, int type, void **cache_slot) {
    php_parallel_exception(
        "%s objects do not support properties",
        ZSTR_VAL(object->ce->name));

    return &EG(uninitialized_zval);
}

const zend_object_handlers* php_parallel_standard_handlers() {
    return &_php_parallel_standard_handlers;
}

PHP_MINIT_FUNCTION(PARALLEL_HANDLERS)
{
    memcpy(
        &_php_parallel_standard_handlers,
        zend_get_std_object_handlers(),
        sizeof(zend_object_handlers));

    _php_parallel_standard_handlers.read_property = php_parallel_handlers_read_property;
    _php_parallel_standard_handlers.write_property = php_parallel_handlers_write_property;
    _php_parallel_standard_handlers.read_dimension = php_parallel_handlers_read_dimension;
    _php_parallel_standard_handlers.write_dimension = php_parallel_handlers_write_dimension;
    _php_parallel_standard_handlers.get_property_ptr_ptr = php_parallel_handlers_get_property_ptr_ptr;

    _php_parallel_standard_handlers.clone_obj = NULL;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_HANDLERS)
{
    return SUCCESS;
}
#endif
