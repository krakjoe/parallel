/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2022                                  |
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
#ifndef HAVE_PARALLEL_CACHE
#define HAVE_PARALLEL_CACHE

#include "parallel.h"

static struct {
    pthread_mutex_t mutex;
    HashTable       table;
    struct {
        size_t      size;
        size_t      used;
        void       *mem;
        void       *block;
    } memory;
} php_parallel_cache_globals = {PTHREAD_MUTEX_INITIALIZER};

#define PCG(e) php_parallel_cache_globals.e
#define PCM(e) PCG(memory).e

#define PARALLEL_CACHE_CHUNK \
    PARALLEL_PLATFORM_ALIGNED((1024 * 1024) * 8)

/* {{{ */
static zend_always_inline void* php_parallel_cache_alloc(size_t size) {
    void *mem;
    size_t aligned =
        PARALLEL_PLATFORM_ALIGNED(size);

    ZEND_ASSERT(size < PARALLEL_CACHE_CHUNK);

    if ((PCM(used) + aligned) >= PCM(size)) {
        PCM(size) = PARALLEL_PLATFORM_ALIGNED(
            PCM(size) + PARALLEL_CACHE_CHUNK);
        PCM(mem) = (void*) realloc(PCM(mem), PCM(size));

        if (!PCM(mem)) {
            /* out of memory */
            return NULL;
        }

        PCM(block) = (void*)(((char*)PCM(mem)) + PCM(used));
    }

    mem = PCM(block);
    PCM(block) =
        (void*)(((char*)PCM(block)) + aligned);
    PCM(used) += aligned;

    return mem;
}

static zend_always_inline void* php_parallel_cache_copy_mem(void *source, zend_long size) {
    void *destination =
        php_parallel_cache_alloc(size);

    memcpy(destination, source, size);

    return destination;
} /* }}} */

static zend_always_inline HashTable* php_parallel_cache_statics(HashTable *statics) { /* {{{ */
    HashTable *cached = zend_hash_index_find_ptr(&PCG(table), (zend_ulong) statics);

    if (cached) {
        return cached;
    }

    cached = php_parallel_copy_hash_persistent(
                statics,
                php_parallel_copy_string_interned,
                php_parallel_cache_copy_mem);

    return zend_hash_index_update_ptr(&PCG(table), (zend_ulong) statics, cached);
} /* }}} */

static zend_always_inline void php_parallel_cache_type(zend_type *type) { /* {{{ */
    zend_type *single;

    if (!ZEND_TYPE_IS_SET(*type)) {
        return;
    }

    if (ZEND_TYPE_HAS_LIST(*type)) {
        zend_type_list *list = ZEND_TYPE_LIST(*type);

        list = php_parallel_cache_copy_mem(
            	list, ZEND_TYPE_LIST_SIZE(list->num_types));

        if (ZEND_TYPE_USES_ARENA(*type)) {
            ZEND_TYPE_FULL_MASK(*type) &= ~_ZEND_TYPE_ARENA_BIT;
        }
 
        ZEND_TYPE_SET_PTR(*type, list);
    }

    ZEND_TYPE_FOREACH(*type, single) {
        if (ZEND_TYPE_HAS_NAME(*single)) {
            zend_string *name = ZEND_TYPE_NAME(*single);

            ZEND_TYPE_SET_PTR(
                *single,
                php_parallel_copy_string_interned(name));
        }
    } ZEND_TYPE_FOREACH_END();
} /* }}} */


/* {{{ */
static zend_op_array* php_parallel_cache_create(const zend_function *source, zend_bool statics) {
    zend_op_array *cached = php_parallel_cache_copy_mem((void*) source, sizeof(zend_op_array));

    cached->fn_flags |= ZEND_ACC_IMMUTABLE;

    if (statics && cached->static_variables) {
        cached->static_variables =
            php_parallel_cache_statics(cached->static_variables);
    }

#if PHP_VERSION_ID >= 80200
    ZEND_MAP_PTR_INIT(cached->static_variables_ptr, cached->static_variables);
#else
    ZEND_MAP_PTR_INIT(cached->static_variables_ptr, &cached->static_variables);
#endif

    ZEND_MAP_PTR_INIT(cached->run_time_cache, NULL);

#if PHP_VERSION_ID >= 80100
    if (cached->num_dynamic_func_defs) {
    	uint32_t it = 0;
    	
    	cached->dynamic_func_defs = php_parallel_cache_copy_mem(
    	                                cached->dynamic_func_defs,
    	                                sizeof(zend_op_array*) * cached->num_dynamic_func_defs);
    	
    	while (it < cached->num_dynamic_func_defs) {
    	    cached->dynamic_func_defs[it] = 
                (zend_op_array*) php_parallel_cache_create(
    	            (zend_function*) cached->dynamic_func_defs[it], statics);
            it++;
    	}
    }
#endif

    if (!cached->refcount) {
        goto _php_parallel_cached_function_return;
    }

    cached->refcount  = NULL;

    if (cached->last_literal) {
        zval     *literal = cached->literals,
                 *end     = literal + cached->last_literal;
        zval     *slot    = php_parallel_cache_copy_mem(
                                    cached->literals,
                                        sizeof(zval) * cached->last_literal);

        cached->literals = slot;

        while (literal < end) {
            if (Z_TYPE_P(literal) == IS_ARRAY) {
                ZVAL_ARR(slot,
                    php_parallel_copy_hash_persistent(
                        Z_ARRVAL_P(literal),
                        php_parallel_copy_string_interned,
                        php_parallel_cache_copy_mem));
            } else if (Z_TYPE_P(literal) == IS_STRING) {
                ZVAL_STR(slot,
                    php_parallel_copy_string_interned(Z_STR_P(literal)));
            }

	        Z_TYPE_FLAGS_P(slot) &= ~(IS_TYPE_REFCOUNTED|IS_TYPE_COLLECTABLE);
            literal++;
            slot++;
        }
    }

    if (cached->last_var) {
        zend_string **vars = cached->vars;
        uint32_t      it = 0,
                      end = cached->last_var;
        zend_string **heap = php_parallel_cache_alloc(cached->last_var * sizeof(zend_string*));

        while (it < end) {
            heap[it] =
                php_parallel_copy_string_interned(vars[it]);
            it++;
        }
        cached->vars = heap;
    }

    if (cached->last) {
        zend_op *opcodes = php_parallel_cache_copy_mem(cached->opcodes, sizeof(zend_op) * cached->last);
        zend_op *opline  = opcodes,
                *end     = opline + cached->last;

        while (opline < end) {
            if (opline->op1_type == IS_CONST) {
#if ZEND_USE_ABS_CONST_ADDR
                opline->op1.zv = (zval*)((char*)opline->op1.zv + ((char*)cached->literals - (char*)source->op_array.literals));
#else
                opline->op1.constant =
                    (char*)(cached->literals +
                            ((zval*)((char*)(source->op_array.opcodes + (opline - opcodes)) +
                            (int32_t)opline->op1.constant) - source->op_array.literals)) -
                            (char*)opline;
#endif
                if (opline->opcode == ZEND_SEND_VAL
                 || opline->opcode == ZEND_SEND_VAL_EX
                 || opline->opcode == ZEND_QM_ASSIGN) {
                    zend_vm_set_opcode_handler_ex(opline, 0, 0, 0);
                }
            }
            if (opline->op2_type == IS_CONST) {
#if ZEND_USE_ABS_CONST_ADDR
                opline->op2.zv = (zval*)((char*)opline->op2.zv + ((char*)cached->literals - (char*)source->op_array.literals));
#else
                opline->op2.constant =
                    (char*)(cached->literals +
                            ((zval*)((char*)(source->op_array.opcodes + (opline - opcodes)) +
                            (int32_t)opline->op2.constant) - source->op_array.literals)) -
                            (char*)opline;
#endif
            }
#if ZEND_USE_ABS_JMP_ADDR
            switch (opline->opcode) {
                case ZEND_JMP:
                case ZEND_FAST_CALL:
                    opline->op1.jmp_addr = &opcodes[opline->op1.jmp_addr - source->op_array.opcodes];
                break;
                case ZEND_JMPZNZ:
                case ZEND_JMPZ:
                case ZEND_JMPNZ:
                case ZEND_JMPZ_EX:
                case ZEND_JMPNZ_EX:
                case ZEND_JMP_SET:
                case ZEND_COALESCE:
                case ZEND_FE_RESET_R:
                case ZEND_FE_RESET_RW:
                case ZEND_ASSERT_CHECK:
                    opline->op2.jmp_addr = &opcodes[opline->op2.jmp_addr - source->op_array.opcodes];
                    break;

                case ZEND_CATCH:
                    if (!(opline->extended_value & ZEND_LAST_CATCH)) {
                        opline->op2.jmp_addr = &opcodes[opline->op2.jmp_addr - source->op_array.opcodes];
                    }
                    break;
            }
#endif

            opline++;
        }
        cached->opcodes = opcodes;
    }

    if (cached->arg_info) {
        zend_arg_info *it    = cached->arg_info,
                      *end   = it + cached->num_args,
                      *info;

        if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
            it--;
        }
        if (cached->fn_flags & ZEND_ACC_VARIADIC) {
            end++;
        }

        cached->arg_info = info = php_parallel_cache_copy_mem(it, (end - it) * sizeof(zend_arg_info));

         while (it < end) {
            if (info->name) {
                info->name =
                    php_parallel_copy_string_interned(it->name);
            }
            
            php_parallel_cache_type(&info->type);

            info++;
            it++;
        }
        if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
            cached->arg_info++;
        }
    }

    if (cached->try_catch_array) {
        cached->try_catch_array =
            php_parallel_cache_copy_mem(
                cached->try_catch_array,
                    sizeof(zend_try_catch_element) * cached->last_try_catch);
    }

    if (cached->live_range) {
        cached->live_range =
            php_parallel_cache_copy_mem(
                cached->live_range,
                sizeof(zend_live_range) * cached->last_live_range);
    }

    if (cached->function_name)
        cached->function_name =
            php_parallel_copy_string_interned(cached->function_name);

    if (cached->filename)
        cached->filename =
            php_parallel_copy_string_interned(cached->filename);

    if (cached->doc_comment)
        cached->doc_comment =
            php_parallel_copy_string_interned(cached->doc_comment);

_php_parallel_cached_function_return:
    return cached;
} /* }}} */

/* {{{ */
static zend_always_inline zend_function* php_parallel_cache_function_ex(const zend_function *source, zend_bool statics) {
    zend_op_array *cached;
    
    pthread_mutex_lock(&PCG(mutex));

    if ((cached = zend_hash_index_find_ptr(&PCG(table), (zend_ulong) source->op_array.opcodes))) {
        goto _php_parallel_cached_function_return;
    }

    cached = php_parallel_cache_create(source, statics);

    zend_hash_index_add_ptr(
        &PCG(table),
        (zend_ulong) source->op_array.opcodes, 
        cached);

_php_parallel_cached_function_return:
    pthread_mutex_unlock(&PCG(mutex));

    return (zend_function*) cached;
} /* }}} */

zend_function* php_parallel_cache_closure(const zend_function *source, zend_function *closure) { /* {{{ */
    zend_op_array *cache;

    cache =
        (zend_op_array*)
            php_parallel_cache_function_ex(
                (zend_function*) source, 0);

    if (!closure) {
        closure = php_parallel_copy_mem(
            cache, sizeof(zend_op_array), 1);
    } else {
        memcpy(closure, cache, sizeof(zend_op_array));
    }

    if (closure->op_array.static_variables) {
        HashTable *statics =
            ZEND_MAP_PTR_GET(
                source->op_array.static_variables_ptr);

        closure->op_array.static_variables =
            php_parallel_copy_hash_ctor(statics, 1);

#if PHP_VERSION_ID >= 80200
        ZEND_MAP_PTR_INIT(
            closure->op_array.static_variables_ptr,
            closure->op_array.static_variables);
#else
        ZEND_MAP_PTR_INIT(
            closure->op_array.static_variables_ptr,
            &closure->op_array.static_variables);
#endif
    }

    return closure;
} /* }}} */

zend_function* php_parallel_cache_function(const zend_function *source) { /* {{{ */
    return php_parallel_cache_function_ex(source, 1);
} /* }}} */

/* {{{ */
PHP_MINIT_FUNCTION(PARALLEL_CACHE)
{
    zend_hash_init(&PCG(table), 32, NULL, NULL, 1);

    PCM(size) = PARALLEL_CACHE_CHUNK;
    PCM(mem) =
        PCM(block) =
            malloc(PCM(size));

    if (!PCM(mem)) {
        /* out of memory */
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_CACHE)
{
    zend_hash_destroy(&PCG(table));

    if (PCM(mem))
        free(PCM(mem));

    return SUCCESS;
} /* }}} */
#endif
