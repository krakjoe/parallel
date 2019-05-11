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

static void php_parallel_cached_dtor(zval *zv) {
    zend_function *function = Z_PTR_P(zv);

    if (function->op_array.last_literal) {
        zval *literal = function->op_array.literals,
             *end     = literal + function->op_array.last_literal;

        while (literal < end) {
            if (Z_OPT_REFCOUNTED_P(literal)) {
                if (GC_DELREF(Z_COUNTED_P(literal)) == 1) {
                    PARALLEL_ZVAL_DTOR(literal);
                }
            }
            literal++;
        }

        pefree(function->op_array.literals, 1);
    }

    if (function->op_array.last_var) {
        /* don't need to free individual vars,
            they are interned and will be freed */
        pefree(function->op_array.vars, 1);
    }

    if (function->op_array.static_variables) {
        php_parallel_copy_hash_dtor(function->op_array.static_variables, 1);
    }

    pefree(function->op_array.opcodes, 1);
    pefree(function, 1);
}

/* {{{ */
zend_function* php_parallel_cache_function(const zend_function *source) {
    zend_function *copy;

    pthread_mutex_lock(&PCG(mutex));

    if ((copy = zend_hash_index_find_ptr(&PCG(table), (zend_ulong) source->op_array.opcodes))) {
        pthread_mutex_unlock(&PCG(mutex));

        return copy;
    }

    copy = php_parallel_copy_mem((void*) source, sizeof(zend_op_array), 1);
    copy->op_array.refcount  = NULL;
    copy->op_array.fn_flags &= ~ZEND_ACC_CLOSURE;

    if (copy->op_array.static_variables) {
        copy->op_array.static_variables =
            php_parallel_copy_hash_ctor(copy->op_array.static_variables, 1);
    }

#ifdef ZEND_ACC_IMMUTABLE
    copy->common.fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

    if (copy->op_array.last_literal) {
        zval     *literal = copy->op_array.literals,
                 *end     = literal + copy->op_array.last_literal;
        zval     *slot    = copy->op_array.literals =
                                php_parallel_copy_mem(
                                    copy->op_array.literals,
                                        sizeof(zval) * copy->op_array.last_literal, 1);

        while (literal < end) {
            if (Z_OPT_REFCOUNTED_P(literal)) {
                PARALLEL_ZVAL_COPY(slot, literal, 1);
                Z_ADDREF_P(slot);
            }
            literal++;
            slot++;
        }
    }

    if (copy->op_array.last_var) {
        zend_string **vars = copy->op_array.vars;
        uint32_t      it = 0,
                      end = copy->op_array.last_var;
        zend_string **heap = pecalloc(copy->op_array.last_var, sizeof(zend_string*), 1);

        while (it < end) {
            heap[it] =
                php_parallel_string_interned(vars[it]);
            it++;
        }

        copy->op_array.vars = heap;
    }

    if (copy->op_array.last) {
        zend_op *opcodes = php_parallel_copy_mem(copy->op_array.opcodes, sizeof(zend_op) * copy->op_array.last, 1);
        zend_op *opline  = opcodes,
                *end     = opline + copy->op_array.last;

        while (opline < end) {
            if (opline->op1_type == IS_CONST) {
#if ZEND_USE_ABS_CONST_ADDR
                opline->op1.zv = (zval*)((char*)opline->op1.zv + ((char*)copy->op_array.literals - (char*)source->op_array.literals));
#elif PHP_VERSION_ID >= 70300
                opline->op1.constant =
                    (char*)(copy->op_array.literals +
                            ((zval*)((char*)(source->op_array.opcodes + (opline - opcodes)) +
                            (int32_t)opline->op1.constant) - source->op_array.literals)) -
                            (char*)opline;
#endif
            }

            if (opline->op2_type == IS_CONST) {
#if ZEND_USE_ABS_CONST_ADDR
                opline->op2.zv = (zval*)((char*)opline->op2.zv + ((char*)copy->op_array.literals - (char*)source->op_array.literals));
#elif PHP_VERSION_ID >= 70300
                opline->op2.constant =
                    (char*)(copy->op_array.literals +
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

        copy->op_array.opcodes = opcodes;
    }

    zend_hash_index_add_ptr(
        &PCG(table),
        (zend_ulong) source->op_array.opcodes, copy);

    pthread_mutex_unlock(&PCG(mutex));

    return copy;
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
