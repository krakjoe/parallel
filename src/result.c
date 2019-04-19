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
#ifndef HAVE_PARALLEL_GROUP_RESULT
#define HAVE_PARALLEL_GROUP_RESULT

#include "parallel.h"
#include "handlers.h"
#include "result.h"
#include "group.h"

zend_class_entry* php_parallel_group_result_ce;
zend_object_handlers php_parallel_group_result_handlers;

static zend_string *php_parallel_group_result_type;
static zend_string *php_parallel_group_result_source;
static zend_string *php_parallel_group_result_object;
static zend_string *php_parallel_group_result_value;

static zend_always_inline void php_parallel_group_result_set_type(zval *result, php_parallel_group_result_type_t type) {
    zval tmp;
    
    ZVAL_LONG(&tmp, type);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_type, &tmp, NULL);
#else
    {
        zval key;
        
        ZVAL_STR(&key, php_parallel_group_result_type);
        
        zend_std_write_property(result, &key, &tmp, NULL);
    }
#endif
}

static zend_always_inline void php_parallel_group_result_set_source(zval *result, zend_string *source) {
    zval tmp;
    
    ZVAL_STR(&tmp, source);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_source, &tmp, NULL);
#else
    {
        zval key;
        
        ZVAL_STR(&key, php_parallel_group_result_source);
        
        zend_std_write_property(result, &key, &tmp, NULL);
    }
#endif
}

static zend_always_inline void php_parallel_group_result_set_object(zval *result, zend_object *object) {
    zval tmp;
    
    ZVAL_OBJ(&tmp, object);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_object, &tmp, NULL);
#else
    {
        zval key;
        
        ZVAL_STR(&key, php_parallel_group_result_object);
        
        zend_std_write_property(result, &key, &tmp, NULL);
    }
#endif
}

static zend_always_inline void php_parallel_group_result_set_value(zval *result, zval *value) {    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_value, value, NULL);
#else
    {
        zval key;
        
        ZVAL_STR(&key, php_parallel_group_result_value);
        
        zend_std_write_property(result, &key, value, NULL);
    }
#endif
    Z_TRY_DELREF_P(value);
}

void php_parallel_group_result_construct(
        php_parallel_group_t *group,
        php_parallel_group_result_type_t type, 
        zend_string *source,
        zend_object *object,
        zval *value,
        zval *return_value) {
    object_init_ex(return_value, php_parallel_group_result_ce);
    
    php_parallel_group_result_set_type(return_value, type);
    php_parallel_group_result_set_source(return_value, source);
    php_parallel_group_result_set_object(return_value, object);
    
    if (type == PHP_PARALLEL_GROUP_RESULT_READ) {
        php_parallel_group_result_set_value(return_value, value);
    }
    
    zend_hash_del(&group->set, source);
}

PHP_METHOD(Result, __construct)
{
    php_parallel_exception(
        "construction of Group\\Result objects is not allowed");
}

zend_function_entry php_parallel_group_result_methods[] = {
    PHP_ME(Result, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};


void php_parallel_group_result_startup(void) {
    zend_class_entry ce;
    
    INIT_NS_CLASS_ENTRY(ce, "parallel\\Group", "Result", php_parallel_group_result_methods);
	
	php_parallel_group_result_ce = zend_register_internal_class(&ce);
	
	zend_declare_class_constant_long(php_parallel_group_result_ce, ZEND_STRL("Read"), PHP_PARALLEL_GROUP_RESULT_READ);
	zend_declare_class_constant_long(php_parallel_group_result_ce, ZEND_STRL("Write"), PHP_PARALLEL_GROUP_RESULT_WRITE);
	
	php_parallel_group_result_type = 
	    zend_new_interned_string(
	        zend_string_init(ZEND_STRL("type"), 1));
	php_parallel_group_result_source = 
	    zend_new_interned_string(
	        zend_string_init(ZEND_STRL("source"), 1));
	php_parallel_group_result_object = 
	    zend_new_interned_string(
	        zend_string_init(ZEND_STRL("object"), 1));
	php_parallel_group_result_value = 
	    zend_new_interned_string(
	        zend_string_init(ZEND_STRL("value"), 1));
	        
	zend_declare_property_null(php_parallel_group_result_ce, ZEND_STRL("type"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_parallel_group_result_ce, ZEND_STRL("source"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_parallel_group_result_ce, ZEND_STRL("object"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_parallel_group_result_ce, ZEND_STRL("value"), ZEND_ACC_PUBLIC);
}

void php_parallel_group_result_shutdown(void) {
    zend_string_release(php_parallel_group_result_type);
    zend_string_release(php_parallel_group_result_source);
    zend_string_release(php_parallel_group_result_object);
    zend_string_release(php_parallel_group_result_value);
}

#endif
