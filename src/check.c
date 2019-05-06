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
#ifndef HAVE_PARALLEL_CHECK
#define HAVE_PARALLEL_CHECK

#include "parallel.h"

TSRM_TLS struct {
    HashTable checked;
} php_parallel_check_globals;

typedef struct _php_parallel_check_t {
    zend_function *function;
    zend_bool      returns;
} php_parallel_check_t;

#define PCG(e) php_parallel_check_globals.e

static zend_always_inline const char* php_parallel_check_opcode_name(zend_uchar opcode) { /* {{{ */
    switch (opcode) {
        case ZEND_DECLARE_FUNCTION:
            return "function";

        case ZEND_DECLARE_ANON_CLASS:
            return "new class";
        
        default:
            return "class";
    }
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_type_closure(zend_type type) { /* {{{ */
    zend_class_entry *class;

#ifdef ZEND_TYPE_IS_CE
    if (ZEND_TYPE_IS_CE(type)) {
        class = ZEND_TYPE_CE(type);
    } else class = zend_lookup_class(ZEND_TYPE_NAME(type));
#else
    class = zend_lookup_class(ZEND_TYPE_NAME(type));
#endif

    if (class == zend_ce_closure) {
        return 1;
    }

    return 0;
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_arginfo(const zend_function *function) { /* {{{ */
    zend_arg_info *it, *end;
    int argc = 1;

    if (!function->op_array.arg_info) {
        return 1;
    }

    if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
        it = function->op_array.arg_info - 1;

        if (ZEND_TYPE_IS_SET(it->type) && (ZEND_TYPE_CODE(it->type) == IS_OBJECT || ZEND_TYPE_IS_CLASS(it->type))) {
            if (!php_parallel_check_type_closure(it->type)) {
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_return_ce,
                    "illegal return (object) from task");
                return 0;
            }
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
            if (!php_parallel_check_type_closure(it->type)) {
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_parameter_ce,
                    "illegal parameter (object) accepted by task at argument %d", argc);
                return 0;
            }
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

static zend_always_inline zend_bool php_parallel_check_argv(zval *args, uint32_t *argc, zval **error) { /* {{{ */
    zval *arg;

    if (*argc == 0) {
        *argc = 1;
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), arg) {
        if (!PARALLEL_ZVAL_CHECK(arg, error)) {
            return 0;
        }

        (*argc)++;
    } ZEND_HASH_FOREACH_END();

    return 1;
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_use(zend_execute_data *execute_data, const zend_function *function, zend_op *bind) { /* {{{ */
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

zend_function* php_parallel_check_task(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns) { /* {{{ */
    php_parallel_check_t check, *checked;
    zend_op *it, *end;

    if (function->type != ZEND_USER_FUNCTION) {
        php_parallel_exception_ex(
            php_parallel_runtime_error_illegal_function_ce,
            "illegal function type (internal)");
        return 0;
    }

    if (argv && Z_TYPE_P(argv) == IS_ARRAY) {
        uint32_t errat = 0;
        zval *errarg;

        if (!php_parallel_check_argv(argv, &errat, &errarg)) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_parameter_ce,
                "illegal parameter (%s) passed to task at argument %d",
                zend_get_type_by_const(Z_TYPE_P(errarg)), errat);
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

    if (!php_parallel_check_arginfo(function)) {
        return 0;
    }

    memset(&check, 0, sizeof(php_parallel_check_t));

    it  = function->op_array.opcodes;
    end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
            case ZEND_DECLARE_LAMBDA_FUNCTION: {
                zend_string *key;
                zend_function *dependency, *errf;
                zend_uchar erro;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, it, &key, &dependency);

                if (!php_parallel_check_function(dependency, &errf, &erro)) {
                    php_parallel_exception_ex(
                        php_parallel_runtime_error_illegal_instruction_ce,
                        "illegal instruction (%s) in closure on line %d of task",
                        php_parallel_check_opcode_name(erro),
                        errf->op_array.line_start - function->op_array.line_start);
                    return 0;
                }
            } break;

            case ZEND_DECLARE_FUNCTION:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (function) on line %d of task",
                    it->lineno - function->op_array.line_start);
                return 0;

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
                if (php_parallel_check_use(execute_data, function, it)) {
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
            php_parallel_cache_function(function);
    } else {
        check.function = NULL;
    }

    zend_hash_index_add_mem(
        &PCG(checked), (zend_ulong) function->op_array.opcodes, &check, sizeof(php_parallel_check_t));

    return (zend_function*) (check.function ? check.function : function);
} /* }}} */

zend_bool php_parallel_check_resource(zval *zv) { /* {{{ */
    zend_resource *resource = Z_RES_P(zv);

    if (resource->type == php_file_le_stream() ||
        resource->type == php_file_le_pstream()) {
        return 1;
    }

    return 0;
} /* }}} */

zend_bool php_parallel_check_function(const zend_function *function, zend_function **errf, zend_uchar *erro) { /* {{{ */
    zend_op *it = function->op_array.opcodes,
            *end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
            case ZEND_DECLARE_FUNCTION:
            case ZEND_DECLARE_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS_DELAYED:
            case ZEND_DECLARE_ANON_CLASS:
                if (errf) {
                    *errf = (zend_function*) function;                
                }
                if (erro) {
                    *erro = it->opcode;
                }
                return 0;

            case ZEND_DECLARE_LAMBDA_FUNCTION: {
                zend_string *key;
                zend_function *dependency;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, it, &key, &dependency);

                if (!php_parallel_check_function(dependency, errf, erro)) {
                    return 0;
                }
            } break;
        }
        it++;
    }

    return 1;
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_closure(zend_closure_t *closure) { /* {{{ */
    php_parallel_check_t check, *checked = zend_hash_index_find_ptr(&PCG(checked), (zend_ulong) closure->func.op_array.opcodes);
    
    if (checked) {
        return checked->returns;
    }

    checked = &check;

    check.returns = 
        php_parallel_check_function(&closure->func, NULL, NULL);

    zend_hash_index_add_mem(&PCG(checked), (zend_ulong) closure->func.op_array.opcodes, &check, sizeof(php_parallel_check_t));

    return checked->returns;
} /* }}} */

zend_bool php_parallel_check_zval(zval *zv, zval **error) { /* {{{ */
    switch (Z_TYPE_P(zv)) {
        case IS_OBJECT:
            if (PARALLEL_IS_CLOSURE(zv)) {
                if (!php_parallel_check_closure((zend_closure_t*) Z_OBJ_P(zv))) {
                    if (error) {
                        *error = zv;
                    }
                    return 0;
                }
                return 1;
            }

            if (error) {
                *error = zv;
            }
            return 0;

        case IS_ARRAY: {
            zval *el;

            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zv), el) {
                if (!php_parallel_check_zval(el, error)) {
                    if (error) {
                        *error = el;
                    }
                    return 0;
                }
            } ZEND_HASH_FOREACH_END();
        } break;

        case IS_RESOURCE:
            if (php_parallel_check_resource(zv)) {
                return 1;
            }

            if (error) {
                *error = zv;
            }
            return 0;
    }
    return 1;
} /* }}} */

/* {{{ */
static void php_parallel_checked_dtor(zval *zv) {
    efree(Z_PTR_P(zv));
}

void php_parallel_check_startup(void) {
    zend_hash_init(&PCG(checked), 32, NULL, php_parallel_checked_dtor, 0);
}

void php_parallel_check_shutdown(void) {
    zend_hash_destroy(&PCG(checked));
}
/* }}} */
#endif
