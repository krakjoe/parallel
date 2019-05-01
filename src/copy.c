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
#ifndef HAVE_PARALLEL_COPY
#define HAVE_PARALLEL_COPY

#include "parallel.h"

#include "php_streams.h"
#include "php_network.h"

#define PCG(e) php_parallel_copy_globals.e

typedef struct _php_parallel_copy_check_t {
    zend_function *function;
    zend_bool      returns;
} php_parallel_copy_check_t;

typedef struct _php_parallel_copy_g {
    HashTable checked;
    HashTable copied;
    HashTable activated;
    HashTable uncached;
} php_parallel_copy_g;

TSRM_TLS php_parallel_copy_g php_parallel_copy_globals;

static const uint32_t php_parallel_copy_uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static void php_parallel_copy_checked_dtor(zval *zv) {
    efree(Z_PTR_P(zv));
}

static void php_parallel_copy_copied_dtor(zval *zv) {
    php_parallel_copy_function_free(Z_PTR_P(zv), 1);
}

static void php_parallel_copy_uncached_dtor(zval *zv) {
    zend_function *function = Z_PTR_P(zv);

    if (function->op_array.last_literal) {
        zval *literal = function->op_array.literals,
             *end     = literal + function->op_array.last_literal;

        while (literal < end) {
            PARALLEL_ZVAL_DTOR(literal);
            literal++;
        }

        pefree(function->op_array.literals, 1);
    }

    if (function->op_array.last_var) {
        zend_string **var = function->op_array.vars,
                    **end = var + function->op_array.last_var;

        while (var < end) {
            zend_string_release(*var);
            var++;
        }
        pefree(function->op_array.vars, 1);
    }

    if (function->op_array.static_variables) {
        php_parallel_copy_hash_dtor(function->op_array.static_variables, 1);
    }

    pefree(function, 1);
}

void php_parallel_copy_startup(void) {
    zend_hash_init(&PCG(copied),    32, NULL, php_parallel_copy_copied_dtor, 0);
    zend_hash_init(&PCG(checked),   32, NULL, php_parallel_copy_checked_dtor, 0);
    zend_hash_init(&PCG(activated), 32, NULL, NULL, 0);
    zend_hash_init(&PCG(uncached),  32, NULL, php_parallel_copy_uncached_dtor, 0);
}

void php_parallel_copy_shutdown(void) {
    zend_hash_destroy(&PCG(copied));
    zend_hash_destroy(&PCG(checked));
    zend_hash_destroy(&PCG(activated));
    zend_hash_destroy(&PCG(uncached));
}

static zend_always_inline void* php_parallel_copy_mem(void *source, size_t size, zend_bool persistent) {
    void *destination = (void*) pemalloc(size, persistent);

    memcpy(destination, source, size);

    return destination;
}

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent);

static zend_always_inline zend_bool php_parallel_copy_resource_castable(zval *zv) {
    zend_resource *resource = Z_RES_P(zv);

    if (resource->type == php_file_le_stream() ||
        resource->type == php_file_le_pstream()) {
        return 1;
    }

    return 0;
}

static zend_always_inline void php_parallel_copy_resource_cast(zval *dest, zval *source) {
    zend_resource *resource = Z_RES_P(source);
#ifndef _WIN32
    if (resource->type == php_file_le_stream() || resource->type == php_file_le_pstream()) {
        int fd;
        php_stream *stream = zend_fetch_resource2_ex(
                                source, "stream",
                                php_file_le_stream(),
                                php_file_le_pstream());

        if (stream) {
            if (php_stream_cast(stream, PHP_STREAM_AS_FD, (void*)&fd, 0) == SUCCESS) {
                ZVAL_LONG(dest, fd);
                return;
            }
        }
    }
#endif
    ZVAL_NULL(dest);
}

static zend_always_inline zend_string* php_parallel_copy_string(zend_string *source, zend_bool persistent) {
    zend_string *dest =
        zend_string_alloc(
            ZSTR_LEN(source), persistent);

    memcpy(ZSTR_VAL(dest), ZSTR_VAL(source), ZSTR_LEN(source)+1);

    ZSTR_LEN(dest) = ZSTR_LEN(source);
    ZSTR_H(dest)   = ZSTR_H(source);

    return dest;
}

void php_parallel_copy_zval_ctor(zval *dest, zval *source, zend_bool persistent) {
    switch (Z_TYPE_P(source)) {
        case IS_NULL:
        case IS_TRUE:
        case IS_FALSE:
        case IS_LONG:
        case IS_DOUBLE:
        case IS_UNDEF:
            if (source != dest) {
                *dest = *source;
            }
        break;

        case IS_STRING:
            ZVAL_STR(dest, php_parallel_copy_string(Z_STR_P(source), persistent));
        break;

        case IS_ARRAY:
            ZVAL_ARR(dest, php_parallel_copy_hash_ctor(Z_ARRVAL_P(source), persistent));
        break;

        case IS_RESOURCE:
            if (php_parallel_copy_resource_castable(source)) {
                php_parallel_copy_resource_cast(dest, source);
                break;
            }

        default:
            ZVAL_BOOL(dest, zend_is_true(source));
    }
}

static zend_always_inline HashTable* php_parallel_copy_hash_permanent(HashTable *source) {
    HashTable *ht = php_parallel_copy_mem(source, sizeof(HashTable), 1);
    uint32_t idx;

    GC_SET_REFCOUNT(ht, 2);
    GC_SET_PERSISTENT_TYPE(ht, GC_ARRAY);
    GC_ADD_FLAGS(ht, IS_ARRAY_IMMUTABLE);

    ht->pDestructor = PARALLEL_ZVAL_DTOR;

#if PHP_VERSION_ID < 70300
    ht->u.flags |= HASH_FLAG_APPLY_PROTECTION|HASH_FLAG_PERSISTENT;
#endif

    ht->u.flags |= HASH_FLAG_STATIC_KEYS;
        if (ht->nNumUsed == 0) {
#if PHP_VERSION_ID >= 70400
        ht->u.flags = HASH_FLAG_UNINITIALIZED;
#else
        ht->u.flags &= ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED);
#endif
        ht->nNextFreeElement = 0;
        ht->nTableMask = HT_MIN_MASK;
        HT_SET_DATA_ADDR(ht, &php_parallel_copy_uninitialized_bucket);
        return ht;
    }

    ht->nNextFreeElement = 0;
    ht->nInternalPointer = HT_INVALID_IDX;
    HT_SET_DATA_ADDR(ht, php_parallel_copy_mem(HT_GET_DATA_ADDR(ht), HT_USED_SIZE(ht), 1));
    for (idx = 0; idx < ht->nNumUsed; idx++) {
        Bucket *p = ht->arData + idx;
        if (Z_TYPE(p->val) == IS_UNDEF) continue;

        if (ht->nInternalPointer == HT_INVALID_IDX) {
            ht->nInternalPointer = idx;
        }

        if (p->key) {
            p->key = php_parallel_copy_string(p->key, 1);
            ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
        } else if ((zend_long) p->h >= (zend_long) ht->nNextFreeElement) {
            ht->nNextFreeElement = p->h + 1;
        }

        PARALLEL_ZVAL_COPY(&p->val, &p->val, 1);
    }

    return ht;
}

static zend_always_inline HashTable* php_parallel_copy_hash_request(HashTable *source) {
    HashTable *ht = php_parallel_copy_mem(source, sizeof(HashTable), 0);

    GC_SET_REFCOUNT(ht, 1);
    GC_DEL_FLAGS(ht, IS_ARRAY_IMMUTABLE);

    GC_TYPE_INFO(ht) = GC_ARRAY;

#if PHP_VERSION_ID < 70300
    ht->u.flags &= ~HASH_FLAG_PERSISTENT;
#endif

    ht->pDestructor = ZVAL_PTR_DTOR;

    if (ht->nNumUsed == 0) {
        HT_SET_DATA_ADDR(ht, &php_parallel_copy_uninitialized_bucket);
        return ht;
    }

    HT_SET_DATA_ADDR(ht, emalloc(HT_SIZE(ht)));
    memcpy(
        HT_GET_DATA_ADDR(ht),
        HT_GET_DATA_ADDR(source),
        HT_HASH_SIZE(ht->nTableMask));

    if (ht->u.flags & HASH_FLAG_STATIC_KEYS) {
        Bucket *p = ht->arData,
        *q = source->arData,
        *p_end = p + ht->nNumUsed;
        for (; p < p_end; p++, q++) {
            *p = *q;
            if (Z_OPT_REFCOUNTED(p->val)) {
                PARALLEL_ZVAL_COPY(&p->val, &p->val, 0);
            }
        }
    } else {
        Bucket *p = ht->arData,
        *q = source->arData,
        *p_end = p + ht->nNumUsed;
        for (; p < p_end; p++, q++) {
            if (Z_TYPE(q->val) == IS_UNDEF) {
                ZVAL_UNDEF(&p->val);
                continue;
            }

            p->val = q->val;
            p->h = q->h;
            if (q->key) {
                p->key = php_parallel_copy_string(q->key, 0);
            } else {
                p->key = NULL;
            }

            PARALLEL_ZVAL_COPY(&p->val, &p->val, 0);
        }
    }

    return ht;
}

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_hash_permanent(source);
    }
    return php_parallel_copy_hash_request(source);
}

static zend_always_inline zend_bool php_parallel_copying_lexical_reference(zend_execute_data *execute_data, const zend_function *function, zend_op *bind) { /* {{{ */
    zend_op *opline, *end;

    if (EX(func)->type != ZEND_USER_FUNCTION) {
        return 0;
    }

    opline = EX(func)->op_array.opcodes;
    end    = opline + EX(func)->op_array.last;

    while (opline < end) {
        if ((opline->opcode == ZEND_BIND_LEXICAL) &&
#if PHP_VERSION_ID >= 70300
            (opline->extended_value & ZEND_BIND_REF)) {
#else
            (opline->extended_value)) {
#endif
            if (zend_string_equals(
                zend_get_compiled_variable_name((zend_op_array*)function, bind->op1.var),
                zend_get_compiled_variable_name((zend_op_array*)EX(func), opline->op2.var))) {
                return 1;
            }
        }
        opline++;
    }

    return 0;
} /* }}} */

zend_bool php_parallel_copy_arginfo_check(const zend_function *function) { /* {{{ */
    zend_arg_info *it, *end;
    int argc = 1;

    if (!function->op_array.arg_info) {
        return 1;
    }

    if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
        it = function->op_array.arg_info - 1;

        if (ZEND_TYPE_IS_SET(it->type) && (ZEND_TYPE_CODE(it->type) == IS_OBJECT || ZEND_TYPE_IS_CLASS(it->type))) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_return_ce,
                "illegal return (object) from task");
            return 0;
        }

        if (it->pass_by_reference) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_return_ce,
                "illegal return (reference) from task");
            return 0;
        }
    }

    it = function->op_array.arg_info;
    end = it + function->op_array.num_args;

    if (function->common.fn_flags & ZEND_ACC_VARIADIC) {
        end++;
    }

    while (it < end) {
        if (ZEND_TYPE_IS_SET(it->type) && (ZEND_TYPE_CODE(it->type) == IS_OBJECT || ZEND_TYPE_IS_CLASS(it->type))) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_parameter_ce,
                "illegal parameter (object) accepted by task at argument %d", argc);
            return 0;
        }

        if (it->pass_by_reference) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_parameter_ce,
                "illegal parameter (reference) accepted by task at argument %d", argc);
            return 0;
        }
        it++;
        argc++;
    }

    return 1;
} /* }}} */

static zend_bool php_parallel_copy_argv_check(zval *args, uint32_t *argc, zval *error) { /* {{{ */
    zval *arg;

    if (*argc == 0) {
        *argc = 1;
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), arg) {
        if (Z_TYPE_P(arg) == IS_OBJECT) {
            ZVAL_COPY_VALUE(error, arg);
            return 0;
        }

        if (Z_TYPE_P(arg) == IS_ARRAY) {
            if (!php_parallel_copy_argv_check(arg, argc, error)) {
                return 0;
            }
        }

        if (Z_TYPE_P(arg) == IS_RESOURCE) {
            if (!php_parallel_copy_resource_castable(arg)) {
                ZVAL_COPY_VALUE(error, arg);
                return 0;
            }
        }

        (*argc)++;
    } ZEND_HASH_FOREACH_END();

    return 1;
} /* }}} */

/* {{{ */
static zend_function* php_parallel_copy_function_uncached(const zend_function *source) {
    zend_function *copy;

    if ((copy = zend_hash_index_find_ptr(&PCG(uncached), (zend_ulong) source->op_array.opcodes))) {
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
            heap[it] = php_parallel_copy_string(vars[it], 1);
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

    return zend_hash_index_add_ptr(&PCG(uncached), (zend_ulong) source->op_array.opcodes, copy);
} /* }}} */

static void php_parallel_copy_function_push(php_parallel_runtime_t *runtime, const zend_function *function, zend_op *opline, zend_bool lambda) { /* {{{ */
#if PHP_VERSION_ID >= 70300
    zend_string *key = Z_STR_P(RT_CONSTANT(opline, opline->op1) + (lambda ? 0 : 1));
    zval        *zv  = zend_hash_find_ex(EG(function_table), key, 1);
#else
    zend_string *key = Z_STR_P(RT_CONSTANT(&function->op_array, opline->op1) + (lambda ? 0 : 1));
    zval        *zv  = zend_hash_find(EG(function_table), key);
#endif
    zend_function *pushing = Z_FUNC_P(zv);

    if (pushing->op_array.refcount) {
        pushing = php_parallel_copy_function_uncached(pushing);
    }

    php_parallel_runtime_function_push(runtime, key, pushing, lambda);
    {
        zend_op *opline = pushing->op_array.opcodes,
                *end    = opline + pushing->op_array.last;

        while (opline < end) {
            if (opline->opcode == ZEND_DECLARE_LAMBDA_FUNCTION) {
                php_parallel_copy_function_push(runtime, pushing, opline, 1);
            } else if (opline->opcode == ZEND_DECLARE_FUNCTION) {
                php_parallel_copy_function_push(runtime, pushing, opline, 0);
            }
            opline++;
        }
    }
} /* }}} */

zend_function* php_parallel_copy_check(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns) { /* {{{ */
    php_parallel_copy_check_t check = {NULL, 0}, *checked;
    zend_op *it, *end;

    if (function->type != ZEND_USER_FUNCTION) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_illegal_function_ce,
            "illegal function type (internal)");
        return 0;
    }

    if (argv && Z_TYPE_P(argv) == IS_ARRAY) {
        uint32_t errat = 0;
        zval errarg;

        if (!php_parallel_copy_argv_check(argv, &errat, &errarg)) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_parameter_ce,
                "illegal parameter (%s) passed to task at argument %d",
                zend_get_type_by_const(Z_TYPE(errarg)), errat);
            return 0;
        }
    }

    if ((checked = zend_hash_index_find_ptr(&PCG(checked), (zend_ulong) function->op_array.opcodes))) {
        *returns =
            checked->returns;

        if (!*returns) {
            if (EX(opline)->result_type != IS_UNUSED) {
                *returns = 1;
            }
        }

        return (zend_function*) (checked->function ? checked->function : function);
    }

    if (!php_parallel_copy_arginfo_check(function)) {
        return 0;
    }

    it  = function->op_array.opcodes;
    end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
            case ZEND_DECLARE_LAMBDA_FUNCTION: {
                php_parallel_copy_function_push(runtime, function, it, 1);
            } break;

            case ZEND_DECLARE_FUNCTION: {
                php_parallel_copy_function_push(runtime, function, it, 0);
            } break;

            case ZEND_YIELD:
            case ZEND_YIELD_FROM:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (yield) on line %d of task",
                    it->lineno - function->op_array.line_start);
                return 0;

            case ZEND_DECLARE_ANON_CLASS:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (new class) on line %d of task",
                    it->lineno - function->op_array.line_start);
                return 0;

            case ZEND_DECLARE_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS_DELAYED:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (class) on line %d of task",
                    it->lineno - function->op_array.line_start);
                return 0;

            case ZEND_BIND_STATIC:
                if (php_parallel_copying_lexical_reference(execute_data, function, it)) {
                    php_parallel_exception_ex(
                        php_parallel_runtime_error_illegal_instruction_ce,
                        "illegal instruction (lexical reference) in task");
                    return 0;
                }
            break;

            case ZEND_THROW:
            case ZEND_RETURN:
            if (!*returns && it->extended_value != -1) {
                if (EX(opline)->result_type == IS_UNUSED) {
                    php_parallel_exception_ex(
                        php_parallel_runtime_error_illegal_return_ce,
                        "return on line %d of task ignored by caller, "
                        "caller must retain reference to Future",
                        it->lineno - function->op_array.line_start);
                    return 0;
                }
                *returns = 1;
            }
            break;
        }
        it++;
    }

    if (!*returns) {
        if (EX(opline)->result_type != IS_UNUSED) {
            *returns = 1;
        }
    }

    check.returns = *returns;
    if (function->op_array.refcount) {
        check.function =
            php_parallel_copy_function_uncached(function);
    } else {
        check.function = NULL;
    }

    zend_hash_index_add_mem(
        &PCG(checked), (zend_ulong) function->op_array.opcodes, &check, sizeof(php_parallel_copy_check_t));

    return (zend_function*) (check.function ? check.function : function);
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_copy_auto_globals_activate_variables(zend_function *function) {
    zend_string **variables = function->op_array.vars;
    int it = 0,
        end = function->op_array.last_var;

    while (it < end) {
        zend_is_auto_global(variables[it]);
        it++;
    }
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_copy_auto_globals_activate_literals(zend_function *function) {
    zval *literals = function->op_array.literals;
    int it = 0,
        end = function->op_array.last_literal;

    while (it < end) {
        if (Z_TYPE(literals[it]) == IS_STRING) {
            zend_is_auto_global(Z_STR(literals[it]));
        }
        it++;
    }
} /* }}} */

/* {{{ */
static zend_always_inline void php_parallel_copy_auto_globals_activate(zend_function *function) {
    if (zend_hash_index_exists(&PCG(activated), (zend_ulong) function->op_array.opcodes)) {
        return;
    }

    php_parallel_copy_auto_globals_activate_variables(function);
    php_parallel_copy_auto_globals_activate_literals(function);

    zend_hash_index_add_empty_element(&PCG(activated), (zend_ulong) function->op_array.opcodes);
} /* }}} */

zend_function* php_parallel_copy_function(const zend_function *function, zend_bool persistent) { /* {{{ */
    zend_function  *copy;

    if (persistent) {
        if ((copy = zend_hash_index_find_ptr(
            &PCG(copied),
            (zend_ulong) function->op_array.opcodes))) {
            return copy;
        }

        copy = php_parallel_copy_mem((void*) function, sizeof(zend_op_array), persistent);
        copy->op_array.refcount  = NULL;
        copy->op_array.fn_flags &= ~ZEND_ACC_CLOSURE;

        if (copy->op_array.static_variables) {
            copy->op_array.static_variables =
                php_parallel_copy_hash_ctor(copy->op_array.static_variables, 1);
        }

#ifdef ZEND_ACC_IMMUTABLE
        copy->common.fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

        return zend_hash_index_add_ptr(&PCG(copied), (zend_ulong) function->op_array.opcodes, copy);
    } else {
        copy = php_parallel_copy_mem((void*) function, sizeof(zend_op_array), persistent);

        if (copy->op_array.static_variables) {
            copy->op_array.static_variables =
                php_parallel_copy_hash_ctor(copy->op_array.static_variables, 0);
        }

#ifdef ZEND_MAP_PTR_NEW
        ZEND_MAP_PTR_INIT(copy->op_array.static_variables_ptr, &copy->op_array.static_variables);
        ZEND_MAP_PTR_NEW(copy->op_array.run_time_cache);
#else
        copy->op_array.run_time_cache = NULL;
#endif

        php_parallel_copy_auto_globals_activate(copy);

        return copy;
    }
} /* }}} */

void php_parallel_copy_function_free(zend_function *function, zend_bool persistent) { /* {{{ */
    zend_op_array *ops = (zend_op_array*) function;

    if (ops->static_variables) {
        php_parallel_copy_hash_dtor(ops->static_variables, persistent);
    }

    pefree(function, persistent);
} /* }}} */
#endif
