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
} php_parallel_cache_globals = {PTHREAD_MUTEX_INITIALIZER};

#define PCG(e) php_parallel_cache_globals.e

static void php_parallel_cache_zval(zval *zv);

static void php_parallel_cached_dtor(zval *zv) { /* {{{ */
    zend_op_array *cached = (zend_op_array*) Z_PTR_P(zv);

    if (cached->last_literal) {
        zval *literal = cached->literals,
             *end     = literal + cached->last_literal;

        while (literal < end) {
            if (Z_OPT_REFCOUNTED_P(literal)) {
                PARALLEL_ZVAL_DTOR(literal);
            }
            literal++;
        }

        pefree(cached->literals, 1);
    }

    if (cached->last_var) {
        pefree(cached->vars, 1);
    }

    if (cached->arg_info) {
        zend_arg_info *info = cached->arg_info;

        if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
            info--;
        }

        pefree(info, 1);
    }

    if (cached->try_catch_array) {
        pefree(cached->try_catch_array, 1);
    }

    if (cached->live_range) {
        pefree(cached->live_range, 1);
    }

    if (cached->static_variables) {
        php_parallel_copy_hash_dtor(cached->static_variables, 1);
    }

    pefree(cached->opcodes, 1);
    pefree(cached, 1);
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_cache_string(zend_string **string) {
    zend_string *value = *string;

    *string = php_parallel_copy_string_interned(value);

    zend_string_release(value);
}

static zend_always_inline void php_parallel_cache_hash(HashTable *ht) {
    Bucket *p = ht->arData,
           *end = p + ht->nNumUsed;

    while (p < end) {
        if (Z_TYPE(p->val) == IS_UNDEF) {
            continue;
        }

        if (p->key) {
            php_parallel_cache_string(&p->key);
        }

        if (Z_TYPE(p->val) == IS_STRING ||
            Z_TYPE(p->val) == IS_ARRAY) {
            php_parallel_cache_zval(&p->val);
        }
        p++;
    }
}

static void php_parallel_cache_zval(zval *zv) {
    switch (Z_TYPE_P(zv)) {
        case IS_ARRAY: php_parallel_cache_hash(Z_ARRVAL_P(zv)); break;
        case IS_STRING: php_parallel_cache_string(&Z_STR_P(zv)); break;

        EMPTY_SWITCH_DEFAULT_CASE();
    }
}
/* }}} */

/* {{{ */
zend_function* php_parallel_cache_function(const zend_function *source) {
    zend_op_array *cached;

    pthread_mutex_lock(&PCG(mutex));

    if ((cached = zend_hash_index_find_ptr(&PCG(table), (zend_ulong) source->op_array.opcodes))) {
        pthread_mutex_unlock(&PCG(mutex));

        return (zend_function*) cached;
    }

    cached = php_parallel_copy_mem((void*) source, sizeof(zend_op_array), 1);
    cached->refcount  = NULL;
    cached->fn_flags &= ~ZEND_ACC_CLOSURE;

    if (cached->static_variables) {
        cached->static_variables =
            php_parallel_copy_hash_ctor(cached->static_variables, 1);
        php_parallel_cache_hash(cached->static_variables);
    }

#ifdef ZEND_ACC_IMMUTABLE
    cached->fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

    if (cached->last_literal) {
        zval     *literal = cached->literals,
                 *end     = literal + cached->last_literal;
        zval     *slot    = cached->literals =
                                php_parallel_copy_mem(
                                    cached->literals,
                                        sizeof(zval) * cached->last_literal, 1);

        while (literal < end) {
            if (Z_OPT_REFCOUNTED_P(literal)) {
                PARALLEL_ZVAL_COPY(slot, literal, 1);

                if (Z_TYPE_P(slot) == IS_STRING ||
                    Z_TYPE_P(slot) == IS_ARRAY) {
                    php_parallel_cache_zval(slot);
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
        zend_string **heap = pecalloc(cached->last_var, sizeof(zend_string*), 1);

        while (it < end) {
            heap[it] =
                php_parallel_copy_string_interned(vars[it]);
            it++;
        }

        cached->vars = heap;
    }

    if (cached->last) {
        zend_op *opcodes = php_parallel_copy_mem(cached->opcodes, sizeof(zend_op) * cached->last, 1);
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

            zend_vm_set_opcode_handler(opline);
            opline++;
        }

        cached->opcodes = opcodes;
    }

    if (cached->arg_info) {
        uint32_t       count = cached->num_args;
        zend_arg_info *it    = cached->arg_info,
                      *end   = it + count;
        zend_arg_info *info, *current;

        if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
            it--;
            count++;
        }

        if (cached->fn_flags & ZEND_ACC_VARIADIC) {
            end++;
            count++;
        }

        current =
            info =
                php_parallel_copy_mem(it, sizeof(zend_arg_info) * count, 1);

        while (it < end) {
            if (current->name) {
                current->name = php_parallel_copy_string_interned(it->name);
            }

            if (ZEND_TYPE_IS_SET(it->type) && ZEND_TYPE_IS_CLASS(it->type)) {
#ifdef ZEND_TYPE_IS_NAME
                if (ZEND_TYPE_IS_NAME(it->type)) {
#endif
                    current->type = ZEND_TYPE_ENCODE_CLASS(
                                    php_parallel_copy_string_interned(
                                        ZEND_TYPE_NAME(it->type)),
                                    ZEND_TYPE_ALLOW_NULL(it->type));
#ifdef ZEND_TYPE_IS_NAME
                } else {
                    current->type = ZEND_TYPE_ENCODE_CLASS(
                                    php_parallel_copy_string_interned(
                                        ZEND_TYPE_CE(it->type)->name),
                                    ZEND_TYPE_ALLOW_NULL(it->type));
                }
#endif
            }
            current++;
            it++;
        }

        if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
            info++;
        }

        cached->arg_info = info;
    }

    if (cached->try_catch_array) {
        cached->try_catch_array =
            php_parallel_copy_mem(
                cached->try_catch_array,
                    sizeof(zend_try_catch_element) * cached->last_try_catch, 1);
    }

    if (cached->live_range) {
        cached->live_range =
            php_parallel_copy_mem(
                cached->live_range,
                sizeof(zend_live_range) * cached->last_live_range, 1);
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

    zend_hash_index_add_ptr(
        &PCG(table),
        (zend_ulong) source->op_array.opcodes, cached);

    pthread_mutex_unlock(&PCG(mutex));

    return (zend_function*) cached;
} /* }}} */

/* {{{ */
PHP_MINIT_FUNCTION(PARALLEL_CACHE)
{
    zend_hash_init(&PCG(table), 32, NULL, php_parallel_cached_dtor, 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_CACHE)
{
    zend_hash_destroy(&PCG(table));

    return SUCCESS;
} /* }}} */
#endif
