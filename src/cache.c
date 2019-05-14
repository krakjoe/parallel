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

typedef union _php_parallel_cache_align_test {
    void *v;
    double d;
    zend_long l;
} php_parallel_cache_align_test;

#if ZEND_GCC_VERSION >= 2000
# define PARALLEL_PLATFORM_ALIGNMENT \
    (__alignof__(php_parallel_cache_align_test) < 8 ? \
        8 : __alignof__(php_parallel_cache_align_test))
#else
# define PARALLEL_PLATFORM_ALIGNMENT \
    (sizeof(php_parallel_cache_align_test))
#endif
#define PARALLEL_CACHE_ALIGNED(size) \
    ZEND_MM_ALIGNED_SIZE_EX(size, PARALLEL_PLATFORM_ALIGNMENT)
#define PARALLEL_CACHE_CHUNK \
    PARALLEL_CACHE_ALIGNED((1024 * 1024) * 8)

/* {{{ */
static zend_always_inline void* php_parallel_cache_alloc(size_t size) {
    void *mem;
    size_t aligned =
        PARALLEL_CACHE_ALIGNED(size);

    ZEND_ASSERT(size < PARALLEL_CACHE_CHUNK);

    if ((PCM(used) + aligned) >= PCM(size)) {
        PCM(size) = PARALLEL_CACHE_ALIGNED(
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

/* {{{ */
zend_function* php_parallel_cache_function(const zend_function *source) {
    zend_op_array *cached;

    pthread_mutex_lock(&PCG(mutex));

    if ((cached = zend_hash_index_find_ptr(&PCG(table), (zend_ulong) source->op_array.opcodes))) {
        goto _php_parallel_cached_function_return;
    }
    cached = php_parallel_cache_copy_mem((void*) source, sizeof(zend_op_array));

    if (!cached->refcount) {
        cached->fn_flags &= ~ZEND_ACC_CLOSURE;
#ifdef ZEND_ACC_IMMUTABLE
        cached->fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

        if ((cached->static_variables != NULL) &&
           !(GC_FLAGS(cached->static_variables) & IS_ARRAY_IMMUTABLE)) {
            cached->static_variables =
                php_parallel_copy_hash_persistent(
                    cached->static_variables,
                    php_parallel_copy_string_interned,
                    php_parallel_cache_copy_mem);
        }

        goto _php_parallel_cached_function_add;
    }

    cached->refcount  = NULL;
    cached->fn_flags &= ~ZEND_ACC_CLOSURE;
#ifdef ZEND_ACC_IMMUTABLE
    cached->fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

    if (cached->static_variables) {
        cached->static_variables =
            php_parallel_copy_hash_persistent(
                cached->static_variables,
                php_parallel_copy_string_interned,
                php_parallel_cache_copy_mem);
    }

    if (cached->last_literal) {
        zval     *literal = cached->literals,
                 *end     = literal + cached->last_literal;
        zval     *slot    = cached->literals =
                                php_parallel_cache_copy_mem(
                                    cached->literals,
                                        sizeof(zval) * cached->last_literal);

        while (literal < end) {
            if (Z_OPT_REFCOUNTED_P(literal)) {
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
            }
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
#elif PHP_VERSION_ID >= 70300
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
#elif PHP_VERSION_ID >= 70300
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

#ifdef ZEND_LAST_CATCH
                case ZEND_CATCH:
                    if (!(opline->extended_value & ZEND_LAST_CATCH)) {
                        opline->op2.jmp_addr = &opcodes[opline->op2.jmp_addr - source->op_array.opcodes];
                    }
                    break;
#endif
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

            if (ZEND_TYPE_IS_SET(it->type) && ZEND_TYPE_IS_CLASS(it->type)) {
                zend_bool nulls =
                    ZEND_TYPE_ALLOW_NULL(it->type);

                zend_string *name =
                        php_parallel_copy_string_interned(ZEND_TYPE_NAME(it->type));

                info->type = ZEND_TYPE_ENCODE_CLASS(name, nulls);
            }

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

_php_parallel_cached_function_add:
    zend_hash_index_add_ptr(&PCG(table), (zend_ulong) source->op_array.opcodes, cached);

_php_parallel_cached_function_return:
    pthread_mutex_unlock(&PCG(mutex));

#ifdef ZEND_MAP_PTR_INIT
    ZEND_MAP_PTR_INIT(cached->static_variables_ptr, &cached->static_variables);
#endif

    return (zend_function*) cached;
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
