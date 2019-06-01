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
#ifndef HAVE_PARALLEL_COPY_H
#define HAVE_PARALLEL_COPY_H

#if PHP_VERSION_ID < 70300
# define GC_SET_REFCOUNT(ref, rc) (GC_REFCOUNT(ref) = (rc))
# define GC_ADDREF(ref) GC_REFCOUNT(ref)++
# define GC_DELREF(ref) --GC_REFCOUNT(ref)
# define GC_SET_PERSISTENT_TYPE(ref, type) (GC_TYPE_INFO(ref) = type)
# define GC_ADD_FLAGS(ref, flags) GC_FLAGS(ref) |= flags
# define GC_DEL_FLAGS(ref, flags) GC_FLAGS(ref) &= ~flags
# define GC_ARRAY (IS_ARRAY | (GC_COLLECTABLE << GC_FLAGS_SHIFT))
# define GC_IMMUTABLE (1<<1)
#else
# define GC_SET_PERSISTENT_TYPE(ref, type) \
    (GC_TYPE_INFO(ref) = type | (GC_PERSISTENT << GC_FLAGS_SHIFT))
#endif

#define PARALLEL_ZVAL_COPY php_parallel_copy_zval_ctor
#define PARALLEL_ZVAL_DTOR php_parallel_copy_zval_dtor

#if PHP_VERSION_ID >= 70300
# define PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, key, destination) do { \
     zval *_tmp; \
     *key = Z_STR_P(RT_CONSTANT(opline, opline->op1)); \
     _tmp  = zend_hash_find_ex(EG(function_table), *key, 1); \
    *destination = Z_FUNC_P(_tmp); \
  } while(0)
#else
# define PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, key, destination) do { \
     zval *_tmp; \
     *key = Z_STR_P(RT_CONSTANT(&function->op_array, opline->op1)); \
     _tmp  = zend_hash_find(EG(function_table), *key); \
    *destination = Z_FUNC_P(_tmp); \
  } while(0)
#endif

typedef struct _zend_closure_t {
    zend_object       std;
    zend_function     func;
    zval              this_ptr;
    zend_class_entry *called_scope;
    zif_handler       orig_internal_handler;
} zend_closure_t;

static zend_always_inline void* php_parallel_copy_mem(void *source, size_t size, zend_bool persistent) {
    void *destination = (void*) pemalloc(size, persistent);

    memcpy(destination, source, size);

    return destination;
}

zend_function* php_parallel_copy_function(const zend_function *function, zend_bool persistent);

zend_string*   php_parallel_copy_string_interned(zend_string *source);
zend_string*   php_parallel_copy_string(zend_string *source, zend_bool persistent);

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent);
void php_parallel_copy_hash_dtor(HashTable *table, zend_bool persistent);

HashTable *php_parallel_copy_hash_persistent(HashTable *source, zend_string* (*)(zend_string*), void* (*)(void *, zend_long));

void           php_parallel_copy_zval_ctor(zval *dest, zval *source, zend_bool persistent);
void           php_parallel_copy_zval_dtor(zval *zv);

zend_class_entry* php_parallel_copy_scope(zend_class_entry *);

PHP_RINIT_FUNCTION(PARALLEL_COPY);
PHP_RSHUTDOWN_FUNCTION(PARALLEL_COPY);

PHP_MINIT_FUNCTION(PARALLEL_COPY);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_COPY);
#endif
