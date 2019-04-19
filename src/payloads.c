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
#ifndef HAVE_PARALLEL_EVENTS_PAYLOADS
#define HAVE_PARALLEL_EVENTS_PAYLOADS

#include "parallel.h"
#include "handlers.h"
#include "payloads.h"

zend_class_entry* php_parallel_events_payloads_ce;
zend_object_handlers php_parallel_events_payloads_handlers;

typedef struct _php_parallel_events_payloads_t {
    HashTable   table;
    zend_object std;
} php_parallel_events_payloads_t;

static zend_always_inline php_parallel_events_payloads_t* php_parallel_events_payloads_fetch(zend_object *o) {
	return (php_parallel_events_payloads_t*) (((char*) o) - XtOffsetOf(php_parallel_events_payloads_t, std));
}

static zend_always_inline php_parallel_events_payloads_t* php_parallel_events_payloads_from(zval *z) {
	return php_parallel_events_payloads_fetch(Z_OBJ_P(z));
}

static zend_object* php_parallel_events_payloads_create(zend_class_entry *type) {
    php_parallel_events_payloads_t *payloads =
        (php_parallel_events_payloads_t*)
            ecalloc(1, sizeof(php_parallel_events_payloads_t) + zend_object_properties_size(type));
            
    zend_object_std_init(&payloads->std, type);
    
    payloads->std.handlers = &php_parallel_events_payloads_handlers;
    
    zend_hash_init(&payloads->table, 32, NULL, ZVAL_PTR_DTOR, 0);
    
    return &payloads->std;
}

static void php_parallel_events_payloads_destroy(zend_object *zo) {
    php_parallel_events_payloads_t *payloads =
        php_parallel_events_payloads_fetch(zo);
    
    zend_hash_destroy(&payloads->table);    
    
    zend_object_std_dtor(zo);
}

PHP_METHOD(Payloads, add)
{
    php_parallel_events_payloads_t *payloads =
        php_parallel_events_payloads_from(getThis());
    zend_string *target;
    zval        *value;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 2, 2)
        Z_PARAM_STR(target)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected target and value");
        return;
    );
    
    if (!zend_hash_add(&payloads->table, target, value)) {
        php_parallel_exception(
            "payload for %s exists", ZSTR_VAL(target));
        return;
    }
    
    Z_TRY_ADDREF_P(value);
}

PHP_METHOD(Payloads, remove)
{
    php_parallel_events_payloads_t *payloads =
        php_parallel_events_payloads_from(getThis());
    zend_string *target;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_STR(target)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected target");
        return;
    );
    
    if (zend_hash_del(&payloads->table, target) != SUCCESS) {
        php_parallel_exception(
            "payload for %s does not exist", ZSTR_VAL(target));
        return;
    }
}

PHP_METHOD(Payloads, clear)
{
    php_parallel_events_payloads_t *payloads =
        php_parallel_events_payloads_from(getThis());

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 0)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "no parameters expected");
        return;
    );
    
    zend_hash_clean(&payloads->table);   
}

zend_function_entry php_parallel_events_payloads_methods[] = {
    PHP_ME(Payloads, add, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Payloads, remove, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Payloads, clear, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

zval* php_parallel_events_payloads_find(zval *zv, zend_string *target) {
    php_parallel_events_payloads_t *payloads;
    
    if (!zv) {
        return NULL;
    }
    
    payloads = php_parallel_events_payloads_from(zv);

    return zend_hash_find(&payloads->table, target);
}

zend_bool php_parallel_events_payloads_exists(zval *zv, zend_string *target) {
    php_parallel_events_payloads_t *payloads;
    
    if (!zv) {
        return 0;
    }
    
    payloads = php_parallel_events_payloads_from(zv);
    
    return zend_hash_exists(&payloads->table, target); 
}

zend_bool php_parallel_events_payloads_remove(zval *zv, zend_string *target) {
    php_parallel_events_payloads_t *payloads;
    
    if (!zv) {
        return 0;
    }
    
    payloads = php_parallel_events_payloads_from(zv);
        
    return zend_hash_del(&payloads->table, target) == SUCCESS;
}

void php_parallel_events_payloads_startup(void) {
    zend_class_entry ce;
    
    memcpy(
	    &php_parallel_events_payloads_handlers, 
	    php_parallel_standard_handlers(), 
	    sizeof(zend_object_handlers));

	php_parallel_events_payloads_handlers.offset = XtOffsetOf(php_parallel_events_payloads_t, std);
	php_parallel_events_payloads_handlers.free_obj = php_parallel_events_payloads_destroy;

	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Payloads", php_parallel_events_payloads_methods);

	php_parallel_events_payloads_ce = zend_register_internal_class(&ce);
	php_parallel_events_payloads_ce->create_object = php_parallel_events_payloads_create;
	php_parallel_events_payloads_ce->ce_flags |= ZEND_ACC_FINAL; 
}

void php_parallel_events_payloads_shutdown(void) {

}
#endif
