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

zend_bool      php_parallel_check_task(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns);
zend_bool      php_parallel_check_zval(zval *zv, zval **error);
zend_bool      php_parallel_check_function(const zend_function *function, zend_function **errf, zend_uchar *erro);

#define PARALLEL_ZVAL_CHECK php_parallel_check_zval
#define PARALLEL_ZVAL_CHECK_CLOSURES php_parallel_check_zval_closures

#define PARALLEL_ZVAL_CHECK_CLOSURE(zv) \
    (Z_TYPE_P(zv) == IS_OBJECT && Z_OBJCE_P(zv) == zend_ce_closure)

static zend_always_inline zend_bool php_parallel_check_zval_closures(zval *zv) { /* {{{ */
    if (PARALLEL_ZVAL_CHECK_CLOSURE(zv)) {
        return 1;
    }

    if (Z_TYPE_P(zv) == IS_ARRAY) {
        zval *val;

        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zv), val) {
            if (PARALLEL_ZVAL_CHECK_CLOSURE(val)) {
                return 1;
            }
        } ZEND_HASH_FOREACH_END();
    }

    if (Z_TYPE_P(zv) == IS_OBJECT) {
        zend_object *object = Z_OBJ_P(zv);

        if (object->ce->default_properties_count) {
            zval *property = object->properties_table,
                 *end     = property + object->ce->default_properties_count;

            while (property < end) {
                if (PARALLEL_ZVAL_CHECK_CLOSURE(property)) {
                    return 1;
                }
                property++;
            }
        }

        if (object->properties) {
            zval *property;

            ZEND_HASH_FOREACH_VAL(object->properties, property) {
                if (PARALLEL_ZVAL_CHECK_CLOSURE(property)) {
                    return 1;
                }
            } ZEND_HASH_FOREACH_END();
        }
    }

    return 0;
} /* }}} */

PHP_RINIT_FUNCTION(PARALLEL_CHECK);
PHP_RSHUTDOWN_FUNCTION(PARALLEL_CHECK);
#endif
