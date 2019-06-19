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
    HashTable tasks;
    HashTable functions;
    HashTable types;
#if PHP_VERSION_ID >= 70400
    HashTable classes;
#endif
} php_parallel_check_globals;

#define PCG(e) php_parallel_check_globals.e

typedef struct _php_parallel_check_task_t {
    zend_function *function;
    zend_bool      returns;
} php_parallel_check_task_t;

typedef struct _php_parallel_check_function_t {
    zend_function *function;
    zend_uchar     instruction;
    zend_bool      valid;
} php_parallel_check_function_t;

typedef struct _php_parallel_check_type_t {
    zend_bool valid;
} php_parallel_check_type_t;

#if PHP_VERSION_ID >= 70400
typedef enum {
    PHP_PARALLEL_CHECK_CLASS_UNKNOWN,
    PHP_PARALLEL_CHECK_CLASS_VALID,
    PHP_PARALLEL_CHECK_CLASS_INVALID,
    PHP_PARALLEL_CHECK_CLASS_INVALID_PROPERTY
} php_parallel_check_class_result_t;

typedef struct _php_parallel_check_class_t {
    php_parallel_check_class_result_t result;
} php_parallel_check_class_t;
#endif

static zend_always_inline const char* php_parallel_check_opcode_name(zend_uchar opcode) { /* {{{ */
    switch (opcode) {
        case ZEND_DECLARE_CLASS:
#ifdef ZEND_DECLARE_INHERITED_CLASS
        case ZEND_DECLARE_INHERITED_CLASS:
        case ZEND_DECLARE_INHERITED_CLASS_DELAYED:
#else
        case ZEND_DECLARE_CLASS_DELAYED:
#endif
            return "class";

        case ZEND_DECLARE_ANON_CLASS:
            return "new class";

        case ZEND_YIELD:
        case ZEND_YIELD_FROM:
            return "yield";

        case ZEND_DECLARE_FUNCTION:
            return "function";

        default:
            return "class";
    }
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_type(zend_type type) { /* {{{ */
    zend_string      *name = ZEND_TYPE_NAME(type);
    zend_class_entry *class;
    php_parallel_check_type_t check, *checked;

    if ((checked = zend_hash_find_ptr(&PCG(types), name))) {
        return checked->valid;
    }

    class = zend_lookup_class(name);

    if (!class) {
        return 0;
    }

    memset(&check, 0, sizeof(php_parallel_check_type_t));

    if (class == zend_ce_closure ||
        class == php_parallel_channel_ce ||
        instanceof_function(class, php_parallel_sync_ce) ||
        !class->create_object) {

        check.valid = 1;
    }

    zend_hash_add_mem(&PCG(types), name, &check, sizeof(php_parallel_check_type_t));

    return check.valid;
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
            if (!php_parallel_check_type(it->type)) {
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_return_ce,
                    "illegal return (%s) from task",
                    ZSTR_VAL(ZEND_TYPE_NAME(it->type)));
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
            if (!php_parallel_check_type(it->type)) {
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_parameter_ce,
                    "illegal parameter (%s) accepted by task at argument %d",
                    ZSTR_VAL(ZEND_TYPE_NAME(it->type)), argc);
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

static zend_always_inline zend_bool php_parallel_check_statics(const zend_function *function, zend_string **errn, zval **errz) { /* {{{ */
    HashTable *statics;
    zval *value, *error;
    zend_string *name;

    if (!function->op_array.static_variables) {
        return 1;
    }

#if PHP_VERSION_ID >= 70400
    statics = ZEND_MAP_PTR_GET(function->op_array.static_variables_ptr);
    if (!statics) {
        statics = function->op_array.static_variables;
    }
#else
    statics = function->op_array.static_variables;
#endif

    ZEND_HASH_FOREACH_STR_KEY_VAL(statics, name, value) {
        if (!PARALLEL_ZVAL_CHECK(value, &error)) {
            if (errn) {
                *errn = name;
            }
            if (errz) {
                *errz = error;
            }
            return 0;
        }
    } ZEND_HASH_FOREACH_END();

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

zend_bool php_parallel_check_task(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns) { /* {{{ */
    php_parallel_check_task_t check, *checked;
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
                Z_TYPE_P(errarg) == IS_OBJECT ?
                    ZSTR_VAL(Z_OBJCE_P(errarg)->name) :
                    zend_get_type_by_const(Z_TYPE_P(errarg)), errat);
            return 0;
        }
    }

    if ((checked = zend_hash_index_find_ptr(&PCG(tasks), (zend_ulong) function->op_array.opcodes))) {
        *returns =
            checked->returns;

        if (!*returns) {
            if (EX(opline)->result_type != IS_UNUSED) {
                *returns = 1;
            }
        }

        return 1;
    }

    if (!php_parallel_check_arginfo(function)) {
        return 0;
    }

    {
        zend_string *errn;
        zval        *errv;
        if (!php_parallel_check_statics(function, &errn, &errv)) {
            php_parallel_exception_ex(
                php_parallel_runtime_error_illegal_variable_ce,
                "illegal variable (%s) named %s in static scope of function",
                Z_TYPE_P(errv) == IS_OBJECT ?
                    ZSTR_VAL(Z_OBJCE_P(errv)->name) :
                    zend_get_type_by_const(Z_TYPE_P(errv)),
                ZSTR_VAL(errn));
            return 0;
        }
    }

    memset(&check, 0, sizeof(php_parallel_check_task_t));

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

                {
                    zend_string *errn;
                    zval        *errv;
                    if (!php_parallel_check_statics(dependency, &errn, &errv)) {
                        php_parallel_exception_ex(
                            php_parallel_runtime_error_illegal_variable_ce,
                            "illegal variable (%s) named %s in static scope of dependency",
                            Z_TYPE_P(errv) == IS_OBJECT ?
                                ZSTR_VAL(Z_OBJCE_P(errv)->name) :
                                zend_get_type_by_const(Z_TYPE_P(errv)),
                            ZSTR_VAL(errn));
                        return 0;
                    }
                }
            } break;

            case ZEND_DECLARE_FUNCTION:
            case ZEND_DECLARE_CLASS:
            case ZEND_DECLARE_ANON_CLASS:
#ifdef ZEND_DECLARE_INHERITED_CLASS
            case ZEND_DECLARE_INHERITED_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS_DELAYED:
#else
            case ZEND_DECLARE_CLASS_DELAYED:
#endif
            case ZEND_YIELD:
            case ZEND_YIELD_FROM:
                php_parallel_exception_ex(
                    php_parallel_runtime_error_illegal_instruction_ce,
                    "illegal instruction (%s) on line %d of task",
                    php_parallel_check_opcode_name(it->opcode),
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

    check.returns = *returns;

    if (!*returns) {
        if (EX(opline)->result_type != IS_UNUSED) {
            *returns = 1;
        }
    }

    zend_hash_index_add_mem(
        &PCG(tasks),
        (zend_ulong) function->op_array.opcodes,
        &check, sizeof(php_parallel_check_task_t));

    return 1;
} /* }}} */

zend_bool php_parallel_check_function(const zend_function *function, zend_function **errf, zend_uchar *erro) { /* {{{ */
    php_parallel_check_function_t check, *checked;
    zend_op *it, *end;

    if ((checked = zend_hash_index_find_ptr(&PCG(functions), (zend_ulong) function->op_array.opcodes))) {
        check =
            *checked;

        goto _php_parallel_checked_function_return;
    }

    memset(&check, 0, sizeof(php_parallel_check_function_t));

    it  = function->op_array.opcodes;
    end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
            case ZEND_DECLARE_FUNCTION:
            case ZEND_DECLARE_CLASS:
#ifdef ZEND_DECLARE_INHERITED_CLASS
            case ZEND_DECLARE_INHERITED_CLASS:
            case ZEND_DECLARE_INHERITED_CLASS_DELAYED:
#else
            case ZEND_DECLARE_CLASS_DELAYED:
#endif
            case ZEND_DECLARE_ANON_CLASS:
                check.function =
                    (zend_function*) function;
                check.instruction = it->opcode;

                goto _php_parallel_checked_function_add;


            case ZEND_DECLARE_LAMBDA_FUNCTION: {
                zend_string *key;
                zend_function *dependency;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, it, &key, &dependency);

                if (!php_parallel_check_function(dependency, &check.function, &check.instruction)) {
                    goto _php_parallel_checked_function_add;
                }
            } break;
        }
        it++;
    }

    check.valid = 1;

_php_parallel_checked_function_add:
    zend_hash_index_add_mem(
        &PCG(functions),
        (zend_ulong) function->op_array.opcodes,
        &check, sizeof(php_parallel_check_function_t));

_php_parallel_checked_function_return:
    if (errf) {
        *errf = check.function;
    }

    if (erro) {
        *erro = check.instruction;
    }

    return check.valid;
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_closure(zend_closure_t *closure) { /* {{{ */
    return php_parallel_check_statics(&closure->func, NULL, NULL) &&
           php_parallel_check_function(&closure->func, NULL, NULL);
} /* }}} */

#if PHP_VERSION_ID >= 70400
static php_parallel_check_class_result_t php_parallel_check_class(zend_class_entry *ce);

static zend_always_inline php_parallel_check_class_result_t php_parallel_check_class_inline(zend_class_entry *ce) {
    zend_property_info *info;
    php_parallel_check_class_t check, *checked = zend_hash_index_find_ptr(&PCG(classes), (zend_ulong) ce);

    if (checked) {
        return checked->result;
    }

    memset(&check, 0, sizeof(php_parallel_check_class_t));

    if (ce == php_parallel_channel_ce ||
        ce == zend_ce_closure) {
        check.result =
            PHP_PARALLEL_CHECK_CLASS_VALID;

        goto _php_parallel_checked_class;
    }

    if (ce->create_object) {
        check.result =
            PHP_PARALLEL_CHECK_CLASS_INVALID;

        goto _php_parallel_checked_class;
    }

    if (!ce->default_properties_count) {
        check.result =
            PHP_PARALLEL_CHECK_CLASS_VALID;

        goto _php_parallel_checked_class;
    }

    ZEND_HASH_FOREACH_PTR(&ce->properties_info, info) {
        zend_class_entry *next;

        if (!ZEND_TYPE_IS_SET(info->type)) {
            check.result =
                PHP_PARALLEL_CHECK_CLASS_UNKNOWN;

            goto _php_parallel_checked_class;
        }

        if (!ZEND_TYPE_IS_CLASS(info->type)) {
            continue;
        }

        if (ZEND_TYPE_IS_CE(info->type)) {
            next = ZEND_TYPE_CE(info->type);
        } else {
            next = zend_lookup_class(
                    ZEND_TYPE_NAME(info->type));
        }

        switch (php_parallel_check_class(next)) {
            case PHP_PARALLEL_CHECK_CLASS_INVALID:
            case PHP_PARALLEL_CHECK_CLASS_INVALID_PROPERTY:
                check.result =
                    PHP_PARALLEL_CHECK_CLASS_INVALID_PROPERTY;
                goto _php_parallel_checked_class;

            case PHP_PARALLEL_CHECK_CLASS_UNKNOWN:
                check.result =
                    PHP_PARALLEL_CHECK_CLASS_UNKNOWN;
                goto _php_parallel_checked_class;

            case PHP_PARALLEL_CHECK_CLASS_VALID:
                /* do nothing */
                break;
        }
    } ZEND_HASH_FOREACH_END();

    check.result =
        PHP_PARALLEL_CHECK_CLASS_VALID;

_php_parallel_checked_class:
    checked = zend_hash_index_add_mem(
        &PCG(classes), (zend_ulong) ce, &check, sizeof(php_parallel_check_class_t));

    return checked->result;
}

static php_parallel_check_class_result_t php_parallel_check_class(zend_class_entry *ce) {
    return php_parallel_check_class_inline(ce);
}
#endif

static zend_always_inline zend_bool php_parallel_check_object(zend_object *object, zval **error) { /* {{{ */
    if (instanceof_function(object->ce, php_parallel_channel_ce) ||
        instanceof_function(object->ce, php_parallel_sync_ce)) {
        return 1;
    }

    if (object->ce->create_object) {
        return 0;
    }

#if PHP_VERSION_ID >= 70400
    if ((object->properties == NULL) || (object->properties->nNumUsed == 0)) {
        switch (php_parallel_check_class_inline(object->ce)) {
            case PHP_PARALLEL_CHECK_CLASS_VALID:
                /* whole graph is typed and allowed */
                return 1;

            case PHP_PARALLEL_CHECK_CLASS_INVALID:
                return 0;

            case PHP_PARALLEL_CHECK_CLASS_INVALID_PROPERTY:
                    /* type info for a property in graph is invalid, but may be undef/null */
            case PHP_PARALLEL_CHECK_CLASS_UNKNOWN:
                    /* not enough type info */
            break;
        }
    }
#endif

    if (object->ce->default_properties_count) {
        zval *property = object->properties_table,
             *end      = property + object->ce->default_properties_count;

        while (property < end) {
            if (!php_parallel_check_zval(property, error)) {
                return 0;
            }
            property++;
        }
    }

    if (object->properties) {
        zval *property;

        ZEND_HASH_FOREACH_VAL(object->properties, property) {
            if (!php_parallel_check_zval(property, error)) {
                return 0;
            }
        } ZEND_HASH_FOREACH_END();
    }

    return 1;
} /* }}} */

static zend_always_inline zend_bool php_parallel_check_resource(zval *zv) { /* {{{ */
    zend_resource *resource = Z_RES_P(zv);

    if (resource->type == php_file_le_stream() ||
        resource->type == php_file_le_pstream()) {
        return 1;
    }

    return 0;
} /* }}} */

zend_bool php_parallel_check_zval(zval *zv, zval **error) { /* {{{ */
    switch (Z_TYPE_P(zv)) {
        case IS_OBJECT:
            if (PARALLEL_ZVAL_CHECK_CLOSURE(zv)) {
                if (!php_parallel_check_closure((zend_closure_t*) Z_OBJ_P(zv))) {
                    if (error) {
                        *error = zv;
                    }
                    return 0;
                }
                return 1;
            } else if (php_parallel_check_object(Z_OBJ_P(zv), error)) {
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

PHP_RINIT_FUNCTION(PARALLEL_CHECK)
{
    zend_hash_init(&PCG(tasks),    32, NULL, php_parallel_checked_dtor, 0);
    zend_hash_init(&PCG(functions),32, NULL, php_parallel_checked_dtor, 0);
    zend_hash_init(&PCG(types),    32, NULL, php_parallel_checked_dtor, 0);
#if PHP_VERSION_ID >= 70400
    zend_hash_init(&PCG(classes),  32, NULL, php_parallel_checked_dtor, 0);
#endif

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_CHECK)
{
#if PHP_VERSION_ID >= 70400
    zend_hash_destroy(&PCG(classes));
#endif
    zend_hash_destroy(&PCG(types));
    zend_hash_destroy(&PCG(functions));
    zend_hash_destroy(&PCG(tasks));

    return SUCCESS;
}
/* }}} */
#endif
