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

typedef struct _zend_closure_t {
    zend_object       std;
    zend_function     func;
    zval              this_ptr;
    zend_class_entry *called_scope;
    zif_handler       orig_internal_handler;
} zend_closure_t;

typedef struct _php_parallel_copy_check_t {
    zend_function *function;
    zend_bool      returns;
} php_parallel_copy_check_t;

TSRM_TLS struct {
    HashTable checked;
    HashTable copied;
    HashTable uncopied;
    HashTable used;
    HashTable activated;
} php_parallel_copy_globals;

static struct {
    pthread_mutex_t mutex;
    HashTable       functions;
    HashTable       dependencies;
} php_parallel_copy_cache;

#define PCG(e) php_parallel_copy_globals.e
#define PCC(e) php_parallel_copy_cache.e

#if PHP_VERSION_ID >= 70300
# define PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, key, destination) do { \
     zval *_tmp; \
     *key = Z_STR_P(RT_CONSTANT(opline, opline->op1) + (opline->opcode == ZEND_DECLARE_LAMBDA_FUNCTION ? 0 : 1)); \
     _tmp  = zend_hash_find_ex(EG(function_table), *key, 1); \
    *destination = Z_FUNC_P(_tmp); \
  } while(0)
#else
# define PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, key, destination) do { \
     zval *_tmp; \
     *key = Z_STR_P(RT_CONSTANT(&function->op_array, opline->op1) + (opline->opcode == ZEND_DECLARE_LAMBDA_FUNCTION ? 0 : 1)); \
     _tmp  = zend_hash_find(EG(function_table), *key); \
    *destination = Z_FUNC_P(_tmp); \
  } while(0)
#endif

static const uint32_t php_parallel_copy_uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static void php_parallel_copy_checked_dtor(zval *zv) {
    efree(Z_PTR_P(zv));
}

static void php_parallel_copy_copied_dtor(zval *zv) {
    php_parallel_copy_function_free(Z_PTR_P(zv), 1);
}

static void php_parallel_copy_uncopied_dtor(zval *zv) {
    zend_string *key = 
        (zend_string*) zend_hash_index_find_ptr(
            &PCG(used), (zend_ulong) Z_FUNC_P(zv));

    if (key) {
        if (zend_hash_exists(EG(function_table), key)) {
            dtor_func_t dtor = /* temporarily remove destructor */
                EG(function_table)->pDestructor;

            EG(function_table)->pDestructor = NULL;

            zend_hash_del(EG(function_table), key);

            EG(function_table)->pDestructor = dtor;
        }
    }

    php_parallel_copy_function_free(Z_PTR_P(zv), 0);
}

static void php_parallel_copy_uncached_dtor(zval *zv) {
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
        zend_string **var = function->op_array.vars,
                    **end = var + function->op_array.last_var;

        while (var < end) {
            if (GC_DELREF(*var) == 1) {
                zend_string_release(*var);
            }
            var++;
        }
        pefree(function->op_array.vars, 1);
    }

    if (function->op_array.static_variables) {
        php_parallel_copy_hash_dtor(function->op_array.static_variables, 1);
    }

    pefree(function->op_array.opcodes, 1);
    pefree(function, 1);
}

void php_parallel_copy_dependencies_dtor(zval *zv) {
    zend_hash_destroy(Z_PTR_P(zv));
    pefree(Z_PTR_P(zv), 1);
}

void php_parallel_copy_cache_startup() {
    pthread_mutexattr_t attributes;

    pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

    pthread_mutex_init(&PCC(mutex), &attributes);
    pthread_mutexattr_destroy(&attributes);

    zend_hash_init(&PCC(functions), 32, NULL, php_parallel_copy_uncached_dtor, 1);
    zend_hash_init(&PCC(dependencies), 32, NULL, php_parallel_copy_dependencies_dtor, 1);
}

void php_parallel_copy_cache_shutdown() {
    zend_hash_destroy(&PCC(functions));
    zend_hash_destroy(&PCC(dependencies));
    pthread_mutex_destroy(&PCC(mutex));
}

void php_parallel_copy_startup(void) {
    zend_hash_init(&PCG(copied),    32, NULL, php_parallel_copy_copied_dtor, 0);
    zend_hash_init(&PCG(uncopied),  32, NULL, php_parallel_copy_uncopied_dtor, 0);
    zend_hash_init(&PCG(used),      32, NULL, NULL, 0);
    zend_hash_init(&PCG(checked),   32, NULL, php_parallel_copy_checked_dtor, 0);
    zend_hash_init(&PCG(activated), 32, NULL, NULL, 0);
}

void php_parallel_copy_shutdown(void) {
    zend_hash_destroy(&PCG(copied));
    zend_hash_destroy(&PCG(uncopied));
    zend_hash_destroy(&PCG(used));
    zend_hash_destroy(&PCG(checked));
    zend_hash_destroy(&PCG(activated));
}

static zend_always_inline void* php_parallel_copy_mem(void *source, size_t size, zend_bool persistent) {
    void *destination = (void*) pemalloc(size, persistent);

    memcpy(destination, source, size);

    return destination;
}

HashTable *php_parallel_copy_hash_ctor(HashTable *source, zend_bool persistent);
void php_parallel_copy_closure(zval *destination, zval *source, zend_bool persistent);

static zend_always_inline zend_bool php_parallel_copy_resource_check(zval *zv) {
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

        case IS_OBJECT:
            if (Z_OBJCE_P(source) == zend_ce_closure) {
                php_parallel_copy_closure(dest, source, persistent);
            } else {
                ZVAL_TRUE(dest);
            }
        break;

        case IS_RESOURCE:
            if (php_parallel_copy_resource_check(source)) {
                php_parallel_copy_resource_cast(dest, source);
                break;
            }

        default:
            ZVAL_BOOL(dest, zend_is_true(source));
    }
}

static zend_always_inline zend_bool php_parallel_copy_use_check(zend_execute_data *execute_data, const zend_function *function, zend_op *bind) { /* {{{ */
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

static zend_bool php_parallel_copy_function_check(const zend_function *function, zend_function **errf, zend_uchar *erro) {
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

                if (!php_parallel_copy_function_check(dependency, errf, erro)) {
                    return 0;
                }
            } break;
        }
        it++;
    }

    return 1;
}

static zend_always_inline zend_bool php_parallel_copy_closure_check(zend_closure_t *closure) {
    php_parallel_copy_check_t check, *checked = zend_hash_index_find_ptr(&PCG(checked), (zend_ulong) closure->func.op_array.opcodes);
    
    if (checked) {
        return checked->returns;
    }

    checked = &check;

    check.returns = 
        php_parallel_copy_function_check(&closure->func, NULL, NULL);

    zend_hash_index_add_mem(&PCG(checked), (zend_ulong) closure->func.op_array.opcodes, &check, sizeof(php_parallel_copy_check_t));

    return checked->returns;
}

zend_bool php_parallel_copy_zval_check(zval *zv, zval **error) {
    switch (Z_TYPE_P(zv)) {
        case IS_OBJECT:
            if (PARALLEL_IS_CLOSURE(zv)) {
                if (!php_parallel_copy_closure_check((zend_closure_t*) Z_OBJ_P(zv))) {
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
                if (!php_parallel_copy_zval_check(el, error)) {
                    if (error) {
                        *error = el;
                    }
                    return 0;
                }
            } ZEND_HASH_FOREACH_END();
        } break;

        case IS_RESOURCE:
            if (php_parallel_copy_resource_check(zv)) {
                return 1;
            }

            if (error) {
                *error = zv;
            }
            return 0;
    }
    return 1;
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

static zend_always_inline zend_bool php_parallel_copy_type_closure(zend_type type) { /* {{{ */
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

zend_bool php_parallel_copy_arginfo_check(const zend_function *function) { /* {{{ */
    zend_arg_info *it, *end;
    int argc = 1;

    if (!function->op_array.arg_info) {
        return 1;
    }

    if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
        it = function->op_array.arg_info - 1;

        if (ZEND_TYPE_IS_SET(it->type) && (ZEND_TYPE_CODE(it->type) == IS_OBJECT || ZEND_TYPE_IS_CLASS(it->type))) {
            if (!php_parallel_copy_type_closure(it->type)) {
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
            if (!php_parallel_copy_type_closure(it->type)) {
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

static zend_bool php_parallel_copy_argv_check(zval *args, uint32_t *argc, zval **error) { /* {{{ */
    zval *arg;

    if (*argc == 0) {
        *argc = 1;
    }

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), arg) {
        if (!PARALLEL_IS_COPYABLE(arg, error)) {
            return 0;
        }

        (*argc)++;
    } ZEND_HASH_FOREACH_END();

    return 1;
} /* }}} */

/* {{{ */
static zend_function* php_parallel_copy_function_uncached(const zend_function *source) {
    zend_function *copy;

    pthread_mutex_lock(&PCC(mutex));

    if ((copy = zend_hash_index_find_ptr(&PCC(functions), (zend_ulong) source->op_array.opcodes))) {
        pthread_mutex_unlock(&PCC(mutex));

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
                php_parallel_copy_string(vars[it], 1);
            GC_ADDREF(heap[it]);
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
        &PCC(functions),
        (zend_ulong) source->op_array.opcodes, copy);

    pthread_mutex_unlock(&PCC(mutex));

    return copy;
} /* }}} */

static zend_always_inline void php_parallel_copy_closure_init_run_time_cache(zend_closure_t *closure) {
#ifdef ZEND_ACC_HEAP_RT_CACHE
    closure->func.op_array.fn_flags |= ZEND_ACC_HEAP_RT_CACHE;
#else
    closure->func.op_array.fn_flags |= ZEND_ACC_NO_RT_ARENA;
#endif

#ifdef ZEND_MAP_PTR_SET
    {
        void *rtc = emalloc(sizeof(void*) + closure->func.op_array.cache_size);

        ZEND_MAP_PTR_INIT(closure->func.op_array.run_time_cache, rtc);
        
        rtc = (char*)rtc + sizeof(void*);
        
        ZEND_MAP_PTR_SET(closure->func.op_array.run_time_cache, rtc);
    }
#else
    closure->func.op_array.run_time_cache = ecalloc(1, closure->func.op_array.cache_size);
#endif
}

void php_parallel_copy_closure(zval *destination, zval *source, zend_bool persistent) { /* {{{ */
    zend_closure_t *copy = php_parallel_copy_mem(Z_OBJ_P(source), sizeof(zend_closure_t), persistent);
    zend_function  *function;

    if (persistent) {
        if (copy->func.op_array.refcount) {
            function =
                php_parallel_copy_function_uncached(&copy->func);
        } else {
            function = (zend_function*) &copy->func;
        }

        function = php_parallel_copy_function(function, 1);

        memcpy(
            &copy->func,
            function,
            sizeof(zend_op_array));

        copy->func.common.fn_flags |= ZEND_ACC_CLOSURE;
    } else {
        function = php_parallel_copy_function(&copy->func, 0);

        memcpy(
            &copy->func,
            function,
            sizeof(zend_op_array));

#ifdef ZEND_ACC_IMMUTABLE
        copy->func.common.fn_flags &= ~ZEND_ACC_IMMUTABLE;
#endif

        if (copy->func.op_array.static_variables) {
            copy->func.op_array.static_variables = 
                zend_array_dup(copy->func.op_array.static_variables);
        }

#ifdef ZEND_MAP_PTR_INIT
        ZEND_MAP_PTR_INIT(copy->func.op_array.static_variables_ptr, &copy->func.op_array.static_variables);
#endif

        php_parallel_copy_closure_init_run_time_cache(copy);

        if (copy->called_scope) {
            copy->called_scope = 
                zend_lookup_class(copy->called_scope->name);
        }

        if (Z_TYPE(copy->this_ptr) == IS_OBJECT) {
            ZVAL_NULL(&copy->this_ptr);
        }

        zend_object_std_init(&copy->std, copy->std.ce);

#if PHP_VERSION_ID < 70300
        copy->func.common.prototype = (void*) copy;
#endif
    }

    ZVAL_OBJ(destination, &copy->std);

    destination->u2.extra = persistent;
} /* }}} */

static zend_always_inline const char* php_parallel_copy_opcode_name(zend_uchar opcode) { /* {{{ */
    switch (opcode) {
        case ZEND_DECLARE_FUNCTION:
            return "function";

        case ZEND_DECLARE_ANON_CLASS:
            return "new class";
        
        default:
            return "class";
    }
} /* }}} */

zend_function* php_parallel_copy_check(php_parallel_runtime_t *runtime, zend_execute_data *execute_data, const zend_function * function, zval *argv, zend_bool *returns) { /* {{{ */
    php_parallel_copy_check_t check, *checked;
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

        if (!php_parallel_copy_argv_check(argv, &errat, &errarg)) {
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

    if (!php_parallel_copy_arginfo_check(function)) {
        return 0;
    }

    memset(&check, 0, sizeof(php_parallel_copy_check_t));

    it  = function->op_array.opcodes;
    end = it + function->op_array.last;

    while (it < end) {
        switch (it->opcode) {
            case ZEND_DECLARE_LAMBDA_FUNCTION: {
                zend_string *key;
                zend_function *dependency, *errf;
                zend_uchar erro;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, it, &key, &dependency);

                if (!php_parallel_copy_function_check(dependency, &errf, &erro)) {
                    php_parallel_exception_ex(
                        php_parallel_runtime_error_illegal_instruction_ce,
                        "illegal instruction (%s) in closure on line %d of task",
                        php_parallel_copy_opcode_name(erro),
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
                if (php_parallel_copy_use_check(execute_data, function, it)) {
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

static void php_parallel_copy_function_dependencies_store(const zend_function *function) { /* {{{ */
    HashTable dependencies;

    pthread_mutex_lock(&PCC(mutex));

    if (zend_hash_index_exists(&PCC(dependencies), (zend_ulong) function->op_array.opcodes)) {

        pthread_mutex_unlock(&PCC(mutex));
        return;
    }

    zend_hash_init(&dependencies, 32, NULL, NULL, 1);
    {
        zend_op *opline = function->op_array.opcodes,
                *end = opline + function->op_array.last;

        while (opline < end) {
            if (opline->opcode == ZEND_DECLARE_FUNCTION ||
                opline->opcode == ZEND_DECLARE_LAMBDA_FUNCTION) {
                zend_string   *key;
                zend_function *dependency;

                PARALLEL_COPY_OPLINE_TO_FUNCTION(function, opline, &key, &dependency);

                if (dependency->op_array.refcount) {
                    dependency = php_parallel_copy_function_uncached(dependency);
                }

                zend_hash_add_ptr(
                    &dependencies,
                    php_parallel_string(key),
                    dependency);

                php_parallel_copy_function_dependencies_store(dependency);
            }
            opline++;
        }
    }
    zend_hash_index_add_mem(&PCC(dependencies), (zend_ulong) function->op_array.opcodes, &dependencies, sizeof(HashTable)); 

    pthread_mutex_unlock(&PCC(mutex));
} /* }}} */

static zend_always_inline void php_parallel_copy_function_dependencies_load(const zend_function *function) { /* {{{ */
    HashTable *dependencies;
    zend_string *key;
    zend_function *dependency;

    pthread_mutex_lock(&PCC(mutex));

    if (!(dependencies = zend_hash_index_find_ptr(&PCC(dependencies), (zend_ulong) function->op_array.opcodes))) {
        pthread_mutex_unlock(&PCC(mutex));
        return;
    }

    pthread_mutex_unlock(&PCC(mutex));

    /* read only table */

    ZEND_HASH_FOREACH_STR_KEY_PTR(dependencies, key, dependency) {
        if (!zend_hash_exists(EG(function_table), key)) {
            dependency = 
                php_parallel_copy_function(dependency, 0);

            zend_hash_add_ptr(EG(function_table), key, dependency);
            zend_hash_index_add_ptr(
                &PCG(used), 
                (zend_ulong) dependency, key);
        }
    } ZEND_HASH_FOREACH_END();
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

        php_parallel_copy_function_dependencies_store(copy);

        return zend_hash_index_add_ptr(&PCG(copied), (zend_ulong) function->op_array.opcodes, copy);
    } else {
        if ((copy = zend_hash_index_find_ptr(
            &PCG(uncopied),
            (zend_ulong) function->op_array.opcodes))) {

            php_parallel_copy_function_dependencies_load(copy);

            return copy;
        }

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

        if (copy->op_array.scope) {
            copy->op_array.scope =
                zend_lookup_class(copy->op_array.scope->name);
        }

        php_parallel_copy_function_dependencies_load(function);

        php_parallel_copy_auto_globals_activate(copy);

        return zend_hash_index_add_ptr(&PCG(uncopied), (zend_ulong) function->op_array.opcodes, copy);
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
