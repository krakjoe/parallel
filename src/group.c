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
#ifndef HAVE_PARALLEL_GROUP
#define HAVE_PARALLEL_GROUP

#include "parallel.h"
#include "handlers.h"
#include "channel.h"
#include "future.h"

#include "ext/standard/php_mt_rand.h"

zend_class_entry* php_parallel_group_ce;
zend_object_handlers php_parallel_group_handlers;

zend_class_entry* php_parallel_group_result_ce;
zend_object_handlers php_parallel_group_result_handlers;

static zend_string *php_parallel_group_result_type;
static zend_string *php_parallel_group_result_source;
static zend_string *php_parallel_group_result_object;
static zend_string *php_parallel_group_result_value;

typedef enum {
    PHP_PARALLEL_GROUP_OK = 0,
    PHP_PARALLEL_GROUP_NOT_OK,
    PHP_PARALLEL_GROUP_NOT_FOUND,
    PHP_PARALLEL_GROUP_NOT_NAMED,
    PHP_PARALLEL_GROUP_NOT_UNIQUE
} php_parallel_group_return;

typedef enum {
    PHP_PARALLEL_GROUP_LINK = 1,
    PHP_PARALLEL_GROUP_FUTURE
} php_parallel_group_type_t;

typedef struct _php_parallel_group_t {
    HashTable   set;
    HashTable   state;
    zend_object std;
} php_parallel_group_t;

typedef struct _php_parallel_group_state_t {
    php_parallel_group_type_t type;
    zend_string *name;
    zend_bool readable;
    zend_bool writable;
    zend_object *object;
} php_parallel_group_state_t;

typedef enum {
    PHP_PARALLEL_GROUP_RESULT_READ = 1,
    PHP_PARALLEL_GROUP_RESULT_WRITE
} php_parallel_group_result_type_t;

static zend_always_inline php_parallel_group_t* php_parallel_group_fetch(zend_object *o) {
	return (php_parallel_group_t*) (((char*) o) - XtOffsetOf(php_parallel_group_t, std));
}

static zend_always_inline php_parallel_group_t* php_parallel_group_from(zval *z) {
	return php_parallel_group_fetch(Z_OBJ_P(z));
}

static void php_parallel_group_result_construct(
                php_parallel_group_t *group,
                php_parallel_group_result_type_t type, 
                zend_string *source,
                zend_object *object,
                zval *value,
                zval *return_value);

void php_parallel_group_state_free(zval *zv) {
    efree(Z_PTR_P(zv));
}

static zend_object* php_parallel_group_create(zend_class_entry *type) {
    php_parallel_group_t *group = 
        (php_parallel_group_t*) ecalloc(1, 
            sizeof(php_parallel_group_t) + zend_object_properties_size(type));
            
    zend_object_std_init(&group->std, type);
    
    group->std.handlers = &php_parallel_group_handlers;
    
    zend_hash_init(&group->set, 32, NULL, ZVAL_PTR_DTOR, 0);
    
    return &group->std;
}

static php_parallel_group_return php_parallel_group_add(php_parallel_group_t *group, zend_string *name, zval *object, zend_string **key) {
    if (Z_TYPE_P(object) != IS_OBJECT) {
        /* ignore noise */
        return PHP_PARALLEL_GROUP_OK;
    }
    
    if (instanceof_function(Z_OBJCE_P(object), php_parallel_future_ce)) {
        if (!name) {
            return PHP_PARALLEL_GROUP_NOT_NAMED;
        }
    } else if(instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
        php_parallel_channel_t *channel = 
            (php_parallel_channel_t*)
                 php_parallel_channel_from(object);
            
        name = php_parallel_link_name(channel->link);
    } else {
        return PHP_PARALLEL_GROUP_NOT_OK;
    }
    
    *key = name;
    
    if (!zend_hash_add(&group->set, name, object)) {
        return PHP_PARALLEL_GROUP_NOT_UNIQUE;
    }
    
    Z_ADDREF_P(object);
    
    return PHP_PARALLEL_GROUP_OK;
}

static php_parallel_group_return php_parallel_group_remove(php_parallel_group_t *group, zend_string *name) {
    zend_long size = 
        zend_hash_num_elements(&group->set);
    
    zend_hash_del(&group->set, name);
    
    if (size == zend_hash_num_elements(&group->set)) {
        return PHP_PARALLEL_GROUP_NOT_FOUND;
    }
    
    return PHP_PARALLEL_GROUP_OK;
}

static zend_always_inline zend_ulong php_parallel_group_perform_begin(php_parallel_group_t *group) {
    zend_string *name;
    zval *object;
    
    zend_hash_init(
        &group->state, 
        zend_hash_num_elements(&group->set), 
        NULL, 
        php_parallel_group_state_free, 0);
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(&group->set, name, object) {
        php_parallel_group_state_t state;
        
        if (instanceof_function(Z_OBJCE_P(object), php_parallel_channel_ce)) {
            php_parallel_channel_t *channel = 
                php_parallel_channel_from(object);
            
            php_parallel_link_lock(channel->link);
            
            state.type     = PHP_PARALLEL_GROUP_LINK;
            state.name     = name;
            state.readable = php_parallel_link_readable(channel->link);
            state.writable = php_parallel_link_writable(channel->link);
            state.object   = Z_OBJ_P(object);
            
            if (state.readable || state.writable) {
                zend_hash_next_index_insert_mem(
                    &group->state,
                    &state, 
                    sizeof(php_parallel_group_state_t));
            } else {
                php_parallel_link_unlock(channel->link);
            }
        } else {
            php_parallel_future_t *future = 
                php_parallel_future_from(object);
            
            php_parallel_future_lock(future);
            
            state.type     = PHP_PARALLEL_GROUP_FUTURE;
            state.name     = name;
            state.readable = php_parallel_future_readable(future);
            state.object   = Z_OBJ_P(object);
            
            if (state.readable) {
                zend_hash_next_index_insert_mem(
                    &group->state,
                    &state, 
                    sizeof(php_parallel_group_state_t));
            } else {
                php_parallel_future_unlock(future);
            }
        }
    } ZEND_HASH_FOREACH_END();
    
    return zend_hash_num_elements(&group->state);
}

static zend_always_inline void php_parallel_group_perform_end(php_parallel_group_t *group) {
    php_parallel_group_state_t *state;
    
    ZEND_HASH_FOREACH_PTR(&group->state, state) {
        if (state->type == PHP_PARALLEL_GROUP_LINK) {
            php_parallel_channel_t *channel = 
                php_parallel_channel_fetch(state->object);

            php_parallel_link_unlock(channel->link);
        } else {
            php_parallel_future_t *future = 
                php_parallel_future_fetch(state->object);
            
            php_parallel_future_unlock(future);
        }
    } ZEND_HASH_FOREACH_END();
    
    zend_hash_destroy(&group->state);
}

static zend_always_inline zend_bool php_parallel_group_perform_link(
                            php_parallel_group_t *group, 
                            php_parallel_group_state_t *state, 
                            zval *payloads,
                            zval *retval) {
    php_parallel_channel_t *channel = 
        php_parallel_channel_fetch(state->object);
    zval *payload;

    if (payloads && (payload = zend_hash_find(Z_ARRVAL_P(payloads), state->name))) {
        
        if (state->writable) {
        
            if (php_parallel_link_send(channel->link, payload)) {
                
                php_parallel_group_result_construct(
                    group,
                    PHP_PARALLEL_GROUP_RESULT_WRITE,
                    state->name,
                    state->object,
                    NULL,
                    retval);
                
                zend_hash_del(
                    Z_ARRVAL_P(payloads), state->name);
                    
                return 1;
            }
        }
    } else {
        if (state->readable) {
            zval read;
            
            if (php_parallel_link_recv(channel->link, &read)) {
                php_parallel_group_result_construct(
                    group,
                    PHP_PARALLEL_GROUP_RESULT_READ,
                    state->name,
                    state->object,
                    &read,
                    retval);
                
                return 1;
            }
        }
    }
    
    return 0;
}

static zend_always_inline zend_bool php_parallel_group_perform_future(
                            php_parallel_group_t *group, 
                            php_parallel_group_state_t *state, 
                            zval *retval) {
    if (state->readable) {
        zval read;
        
        php_parallel_future_t *future =
            php_parallel_future_fetch(state->object);
            
        php_parallel_future_value(future, &read);
        
        php_parallel_group_result_construct(
            group,
            PHP_PARALLEL_GROUP_RESULT_READ,
            state->name,
            state->object,
            &read,
            retval);
        
        return 1;
    }
    
    return 0;
}

static void php_parallel_group_perform(php_parallel_group_t *group, zval *payloads, zval *retval) {
    if (zend_hash_num_elements(&group->set) == 0) {
        ZVAL_FALSE(retval);
        return;
    }

    do {
        zval *zv;
        php_parallel_group_state_t *state;
        zend_long selected;
        zend_long size = 
            php_parallel_group_perform_begin(group);

        if (size == 0) {
            /* todo timeout */
_php_parallel_group_perform_continue:
            php_parallel_group_perform_end(group);
            continue;
        }
        
        selected = php_mt_rand_range(
            0, 
            size - 1);
            
        ZEND_HASH_INDEX_FIND(
            (&group->state), 
            (selected), 
            (zv), 
            _php_parallel_group_perform_continue);
        
        state = Z_PTR_P(zv);
        
        if (state->type == PHP_PARALLEL_GROUP_LINK) {
            if (php_parallel_group_perform_link(
                    group, 
                    state,
                    payloads,
                    retval)) {
                break;        
            }
        } else {
            if (php_parallel_group_perform_future(
                    group, 
                    state, 
                    retval)) {
                break;        
            }
        }
        
        php_parallel_group_perform_end(group);  
    } while (1);
    
    php_parallel_group_perform_end(group);  
}

static void php_parallel_group_destroy(zend_object *zo) {
    php_parallel_group_t *group =
        php_parallel_group_fetch(zo);
        
    zend_hash_destroy(&group->set);
    
    zend_object_std_dtor(zo);
}

static zend_always_inline void php_parallel_group_result_set_type(zval *result, php_parallel_group_result_type_t type) {
    zval tmp;
    
    ZVAL_LONG(&tmp, type);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_type, &tmp, NULL);
#else
    zend_std_write_property(result, php_parallel_group_result_type, &tmp, NULL);
#endif
}

static zend_always_inline void php_parallel_group_result_set_source(zval *result, zend_string *source) {
    zval tmp;
    
    ZVAL_STR(&tmp, source);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_source, &tmp, NULL);
#else
    zend_std_write_property(result, php_parallel_group_result_source, &tmp, NULL);
#endif
}

static zend_always_inline void php_parallel_group_result_set_object(zval *result, zend_object *object) {
    zval tmp;
    
    ZVAL_OBJ(&tmp, object);
    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_object, &tmp, NULL);
#else
    zend_std_write_property(result, php_parallel_group_result_object, &tmp, NULL);
#endif
}

static zend_always_inline void php_parallel_group_result_set_value(zval *result, zval *value) {    
#if PHP_VERSION_ID >= 80000
    zend_std_write_property(Z_OBJ_P(result), php_parallel_group_result_value, value, NULL);
#else
    zend_std_write_property(result, php_parallel_group_result_value, value, NULL);
#endif
    Z_TRY_DELREF_P(value);
}

static void php_parallel_group_result_construct(
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

PHP_METHOD(Group, __construct)
{
    php_parallel_group_t *group = php_parallel_group_from(getThis());
    zval *set = NULL;
    zend_string *name = NULL;
    zval *object = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(set)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception("expected optional array of objects");
        return;
    );
    
    if (!set) {
        return;
    }
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(set), name, object) {
        zend_string *key;
        
        switch (php_parallel_group_add(group, name, object, &key)) {
            case PHP_PARALLEL_GROUP_OK:
                break;
            
            case PHP_PARALLEL_GROUP_NOT_OK:
                php_parallel_exception(
                    "object of class %s cannot be added to \\parallel\\Group", 
                    ZSTR_VAL(Z_OBJCE_P(object)->name));
                return;
                
            case PHP_PARALLEL_GROUP_NOT_NAMED:
                php_parallel_exception(
                    "encountered \\parallel\\Future without name");
                return;
                
            case PHP_PARALLEL_GROUP_NOT_UNIQUE:
                php_parallel_exception(
                    "object named \"%s\" already added", 
                    ZSTR_VAL(key));
                return;
            
            EMPTY_SWITCH_DEFAULT_CASE();
                
        }
    } ZEND_HASH_FOREACH_END();
}

PHP_METHOD(Group, add)
{
    php_parallel_group_t *group = php_parallel_group_from(getThis());
    zend_string *name = NULL, 
                *key = NULL;
    zval *object = NULL;
    
    switch (ZEND_NUM_ARGS()) {
        case 2:
            ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 2, 2)
                Z_PARAM_STR(name)
                Z_PARAM_OBJECT_OF_CLASS(object, php_parallel_future_ce)
            ZEND_PARSE_PARAMETERS_END_EX(
                php_parallel_exception(
                    "expected string and \\parallel\\Future");
                return;
            );
        break;
        
        case 1:
            ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
                Z_PARAM_OBJECT_OF_CLASS(object, php_parallel_channel_ce)
            ZEND_PARSE_PARAMETERS_END_EX(
                php_parallel_exception(
                    "expected \\parallel\\Channel");
                return;
            );
        break;
        
        default:
            php_parallel_exception(
                "expected string and \\parallel\\Future, or \\parallel\\Channel");
            return;
    }
    
    switch (php_parallel_group_add(group, name, object, &key)) {
        case PHP_PARALLEL_GROUP_NOT_UNIQUE:
            php_parallel_exception("object named \"%s\" already added", ZSTR_VAL(key));
            return;
        
        case PHP_PARALLEL_GROUP_OK:
            break;
     
        EMPTY_SWITCH_DEFAULT_CASE();     
    }
}

PHP_METHOD(Group, remove)
{
    php_parallel_group_t *group = php_parallel_group_from(getThis());
    zend_string *name;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception("expected string");
        return;
    );
    
    switch (php_parallel_group_remove(group, name)) {
        case PHP_PARALLEL_GROUP_OK:
            break;
            
        case PHP_PARALLEL_GROUP_NOT_FOUND:
            php_parallel_exception(
                "object named \"%s\" not found", ZSTR_VAL(name));
            return;
            
        EMPTY_SWITCH_DEFAULT_CASE();
    }
}

ZEND_BEGIN_ARG_INFO_EX(php_parallel_group_perform_arginfo, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(1, payloads, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Group, perform)
{
    php_parallel_group_t *group = php_parallel_group_from(getThis());
    zval *payloads = NULL;
    
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_QUIET, 0, 1)
        Z_PARAM_OPTIONAL
	    Z_PARAM_ARRAY_EX2(payloads, 1, 1, 1)
    ZEND_PARSE_PARAMETERS_END_EX(
        php_parallel_exception(
            "expected optional payloads array");
        return;
    );
    
    if (zend_hash_num_elements(&group->set) == 0) {
        RETURN_FALSE;
    }
    
    php_parallel_group_perform(group, payloads, return_value);
}

zend_function_entry php_parallel_group_methods[] = {
    PHP_ME(Group, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Group, add, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Group, remove, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Group, perform, php_parallel_group_perform_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_METHOD(Result, __construct)
{
    php_parallel_exception(
        "construction of Group\\Result objects is not allowed");
}

zend_function_entry php_parallel_group_result_methods[] = {
    PHP_ME(Result, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void php_parallel_group_startup(void) {
    zend_class_entry ce;
    
    memcpy(
	    &php_parallel_group_handlers, 
	    php_parallel_standard_handlers(), 
	    sizeof(zend_object_handlers));

	php_parallel_group_handlers.offset = XtOffsetOf(php_parallel_group_t, std);
	php_parallel_group_handlers.free_obj = php_parallel_group_destroy;

	INIT_NS_CLASS_ENTRY(ce, "parallel", "Group", php_parallel_group_methods);

	php_parallel_group_ce = zend_register_internal_class(&ce);
	php_parallel_group_ce->create_object = php_parallel_group_create;
	php_parallel_group_ce->ce_flags |= ZEND_ACC_FINAL;
	
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

void php_parallel_group_shutdown(void) {
    zend_string_release(php_parallel_group_result_type);
    zend_string_release(php_parallel_group_result_source);
    zend_string_release(php_parallel_group_result_object);
    zend_string_release(php_parallel_group_result_value);
}
#endif
