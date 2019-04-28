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

extern zend_string* php_parallel_runtime_main;

typedef struct _php_parallel_copy_g {
    HashTable checked;
    HashTable copied;
    HashTable activated;
} php_parallel_copy_g;

TSRM_TLS php_parallel_copy_g php_parallel_copy_globals;

#define PCG(e) php_parallel_copy_globals.e

static const uint32_t uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static void php_parallel_copy_dtor(zval *zv) {
    php_parallel_copy_free(Z_PTR_P(zv), 1);    
}

static void php_parallel_check_dtor(zval *zv) {
    efree(Z_PTR_P(zv));
}

void php_parallel_copy_startup(void) {
    zend_hash_init(&PCG(copied),    32, NULL, php_parallel_copy_dtor, 0);
    zend_hash_init(&PCG(checked),   32, NULL, php_parallel_check_dtor, 0);
    zend_hash_init(&PCG(activated), 32, NULL, NULL, 0);
}

void php_parallel_copy_shutdown(void) {
    zend_hash_destroy(&PCG(copied));
    zend_hash_destroy(&PCG(checked));
    zend_hash_destroy(&PCG(activated));
}

static zend_always_inline void* php_parallel_copy_mem(void *source, size_t size, zend_bool persistent) {
    void *destination = (void*) pemalloc(size, persistent);

    memcpy(destination, source, size);

    return destination;
}

HashTable *php_parallel_copy_hash(HashTable *source, zend_bool persistent);

static zend_always_inline zend_bool php_parallel_resource_castable(zval *zv) {
    zend_resource *resource = Z_RES_P(zv);

    if (resource->type == php_file_le_stream() ||
        resource->type == php_file_le_pstream()) {
        return 1;
    }

    return 0;
}

static zend_always_inline void php_parallel_cast_resource(zval *dest, zval *source) {
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

void php_parallel_copy_zval(zval *dest, zval *source, zend_bool persistent) {
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
            ZVAL_ARR(dest, php_parallel_copy_hash(Z_ARRVAL_P(source), persistent));
        break;

        case IS_RESOURCE:
            if (php_parallel_resource_castable(source)) {
                php_parallel_cast_resource(dest, source);
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

    ht->pDestructor = php_parallel_zval_dtor;

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
        HT_SET_DATA_ADDR(ht, &uninitialized_bucket);
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

        php_parallel_copy_zval(&p->val, &p->val, 1);
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
        HT_SET_DATA_ADDR(ht, &uninitialized_bucket);
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
                php_parallel_copy_zval(&p->val, &p->val, 0);
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

            php_parallel_copy_zval(&p->val, &p->val, 0);
        }
    }

    return ht;
}

HashTable *php_parallel_copy_hash(HashTable *source, zend_bool persistent) {
    if (persistent) {
        return php_parallel_copy_hash_permanent(source);
    }
    return php_parallel_copy_hash_request(source);
}

static zend_always_inline zend_bool php_parallel_copying_lexical(zend_execute_data *execute_data, const zend_function *function, zend_op *bind) { /* {{{ */
    zend_op *opline, *end;

    if (EX(func)->type != ZEND_USER_FUNCTION) {
        return 0;
    }

    opline = EX(func)->op_array.opcodes;
    end    = opline + EX(func)->op_array.last;

    while (opline < end) {
        if (opline->opcode == ZEND_BIND_LEXICAL) {
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
            if (!php_parallel_resource_castable(arg)) {
                ZVAL_COPY_VALUE(error, arg);
                return 0;
            }
        }

        (*argc)++;
    } ZEND_HASH_FOREACH_END();

	return 1;
} /* }}} */

zend_bool php_parallel_copy_check(zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns) { /* {{{ */
    zend_op *it, *end;
    uint32_t errat = 0;
    zval errarg;
    zend_bool *checked = zend_hash_index_find_ptr(
    &PCG(checked), (zend_ulong) function->op_array.opcodes);

    if (checked) {
        *returns = *checked;

        return 1;
    }

    if (function->type != ZEND_USER_FUNCTION) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_illegal_function_ce,
            "illegal function type (internal)");
        return 0;
    }

    if (!php_parallel_copy_arginfo_check(function)) {
        return 0;
    }

    if (argv && Z_TYPE_P(argv) == IS_ARRAY && !php_parallel_copy_argv_check(argv, &errat, &errarg)) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_illegal_parameter_ce,
            "illegal parameter (%s) passed to task at argument %d", 
            zend_get_type_by_const(Z_TYPE(errarg)), errat);
        return 0;
    }

    it  = function->op_array.opcodes;
    end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
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

            case ZEND_DECLARE_LAMBDA_FUNCTION:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (function) on line %d of task",
                    it->lineno - function->op_array.line_start);
                return 0;

            case ZEND_DECLARE_FUNCTION:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (function) on line %d of task",
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
                if (php_parallel_copying_lexical(execute_data, function, it)) {
                    php_parallel_exception_ex(
                        php_parallel_runtime_error_illegal_instruction_ce,
                        "illegal instruction (lexical) in task");
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

    zend_hash_index_add_mem(&PCG(checked), (zend_ulong) function->op_array.opcodes, returns, sizeof(zend_bool));
	return 1;
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

zend_function* php_parallel_copy(const zend_function *function, zend_bool persistent) { /* {{{ */
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
                php_parallel_copy_hash(copy->op_array.static_variables, 1);
        }

#ifdef ZEND_ACC_IMMUTABLE
        copy->common.fn_flags |= ZEND_ACC_IMMUTABLE;
#endif

        return zend_hash_index_add_ptr(&PCG(copied), (zend_ulong) function->op_array.opcodes, copy);
    } else {
        copy = php_parallel_copy_mem((void*) function, sizeof(zend_op_array), persistent);

        if (copy->op_array.static_variables) {
            copy->op_array.static_variables = 
                php_parallel_copy_hash(copy->op_array.static_variables, 0);
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

void php_parallel_copy_free(zend_function *function, zend_bool persistent) { /* {{{ */
    zend_op_array *ops = (zend_op_array*) function;

    if (ops->static_variables) {
        php_parallel_ht_dtor(ops->static_variables, persistent);
    }

    pefree(function, persistent);
} /* }}} */
#endif
