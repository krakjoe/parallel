/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2024                                  |
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

#include "parallel.h"

static struct {
	pthread_mutex_t mutex;
	HashTable       functions;
#if PHP_VERSION_ID < 80200
	HashTable statics;
#endif
	struct {
		size_t size;
		size_t used;
		void  *mem;
		void  *block;
	} memory;
} php_parallel_cache_globals = {PTHREAD_MUTEX_INITIALIZER};

#define PCG(e) php_parallel_cache_globals.e
#define PCM(e) PCG(memory).e

#define PARALLEL_CACHE_CHUNK PARALLEL_PLATFORM_ALIGNED((1024 * 1024) * 8)
#define PARALLEL_CACHE_FILE_HASH_OFFSET 1469598103934665603ULL
#define PARALLEL_CACHE_FILE_HASH_PRIME 1099511628211ULL

#if PHP_VERSION_ID < 80200
#define PARALLEL_CACHE_STATICS_PARAM , bool statics
#define PARALLEL_CACHE_STATICS_ARG(value) , value
#else
#define PARALLEL_CACHE_STATICS_PARAM
#define PARALLEL_CACHE_STATICS_ARG(value)
#endif

typedef struct _php_parallel_cache_entry_t {
	zend_op_array *function;
#if PHP_VERSION_ID >= 80400
	uint64_t source;
#endif
} php_parallel_cache_entry_t;

/* {{{ */
static zend_always_inline void *php_parallel_cache_alloc(size_t size)
{
	void  *mem;
	size_t aligned = PARALLEL_PLATFORM_ALIGNED(size);

	ZEND_ASSERT(size < PARALLEL_CACHE_CHUNK);

	if ((PCM(used) + aligned) >= PCM(size)) {
		PCM(size) = PARALLEL_PLATFORM_ALIGNED(PCM(size) + PARALLEL_CACHE_CHUNK);
		PCM(mem) = (void *)realloc(PCM(mem), PCM(size));

		if (!PCM(mem)) {
			/* out of memory */
			return NULL;
		}

		PCM(block) = (void *)(((char *)PCM(mem)) + PCM(used));
	}

	mem = PCM(block);
	PCM(block) = (void *)(((char *)PCM(block)) + aligned);
	PCM(used) += aligned;

	return mem;
}

static zend_always_inline void *php_parallel_cache_copy_mem(void *source, zend_long size)
{
	void *destination = php_parallel_cache_alloc(size);

	memcpy(destination, source, size);

	return destination;
} /* }}} */

#if PHP_VERSION_ID >= 80400
static zend_always_inline uint64_t php_parallel_cache_file_hash(const zend_op_array *source)
{ /* {{{ */
	FILE    *file;
	uint64_t hash = PARALLEL_CACHE_FILE_HASH_OFFSET;
	char     buffer[4096];
	size_t   read;

	if (!source->filename) {
		return 0;
	}

	file = VCWD_FOPEN(ZSTR_VAL(source->filename), "rb");
	if (!file) {
		return 0;
	}

	while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		char *it = buffer, *end = buffer + read;

		while (it < end) {
			hash ^= (unsigned char)*it++;
			hash *= PARALLEL_CACHE_FILE_HASH_PRIME;
		}
	}

	fclose(file);

	return hash;
} /* }}} */
#endif

#if PHP_VERSION_ID < 80200
static zend_always_inline HashTable *php_parallel_cache_statics(HashTable *statics)
{ /* {{{ */
	HashTable *cached = zend_hash_index_find_ptr(&PCG(statics), (zend_ulong)statics);

	if (cached) {
		return cached;
	}

	cached = php_parallel_copy_hash_persistent(statics, php_parallel_copy_string_interned, php_parallel_cache_copy_mem,
	                                           PHP_PARALLEL_COPY_STORAGE_CACHE_POOL);

	return zend_hash_index_update_ptr(&PCG(statics), (zend_ulong)statics, cached);
} /* }}} */
#endif

static zend_always_inline void php_parallel_cache_type(zend_type *type)
{ /* {{{ */
	zend_type *single;

	if (!ZEND_TYPE_IS_SET(*type)) {
		return;
	}

	if (ZEND_TYPE_HAS_LIST(*type)) {
		zend_type_list *list = ZEND_TYPE_LIST(*type);

		list = php_parallel_cache_copy_mem(list, ZEND_TYPE_LIST_SIZE(list->num_types));

		if (ZEND_TYPE_USES_ARENA(*type)) {
			ZEND_TYPE_FULL_MASK(*type) &= ~_ZEND_TYPE_ARENA_BIT;
		}

		ZEND_TYPE_SET_PTR(*type, list);
	}

	ZEND_TYPE_FOREACH(*type, single)
	{
		if (ZEND_TYPE_HAS_NAME(*single)) {
			zend_string *name = ZEND_TYPE_NAME(*single);

			ZEND_TYPE_SET_PTR(*single, php_parallel_copy_string_interned(name));
		}
	}
	ZEND_TYPE_FOREACH_END();
} /* }}} */

/* {{{ */
static zend_op_array *php_parallel_cache_create(const zend_function *source PARALLEL_CACHE_STATICS_PARAM)
{
	zend_op_array *cached = php_parallel_cache_copy_mem((void *)source, sizeof(zend_op_array));
	uint32_t      *literal_map = NULL;
	uint32_t      *offset_map = NULL;
	uint32_t       new_last_literal = cached->last_literal;

	cached->fn_flags |= ZEND_ACC_IMMUTABLE;

#if PHP_VERSION_ID < 80200
	if (statics && cached->static_variables) {
		cached->static_variables = php_parallel_cache_statics(cached->static_variables);
	}
#endif

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
		    cached->dynamic_func_defs, sizeof(zend_op_array *) * cached->num_dynamic_func_defs);

		while (it < cached->num_dynamic_func_defs) {
			cached->dynamic_func_defs[it] = (zend_op_array *)php_parallel_cache_create(
			    (zend_function *)cached->dynamic_func_defs[it] PARALLEL_CACHE_STATICS_ARG(statics));
			it++;
		}
	}
#endif

	if (!cached->refcount) {
		goto _php_parallel_cached_function_return;
	}

	cached->refcount = NULL;

	if (cached->last_literal) {
		zend_op *src_opline = source->op_array.opcodes;
		zend_op *src_end = src_opline + source->op_array.last;

		// A map to keep track of which literals are referenced by the
		// `ZEND_INIT_FCALL` opcodes we found so that we can expand those later
		literal_map = emalloc(sizeof(uint32_t) * cached->last_literal);
		memset(literal_map, 0, sizeof(uint32_t) * cached->last_literal);

		// Search for `ZEND_INIT_FCALL` opcodes and remember the indexes for the
		// literals, as we are rewriting them later to `ZEND_INIT_FCALL_BY_NAME`
		// which requires a second, lower cased literal just in the next literal
		// slot.
		while (src_opline < src_end) {
			if (src_opline->opcode == ZEND_INIT_FCALL && src_opline->op2_type == IS_CONST) {
				uint32_t idx;
#if ZEND_USE_ABS_CONST_ADDR
				idx = (zval *)src_opline->op2.zv - source->op_array.literals;
#else
				idx = ((zval *)((char *)src_opline + src_opline->op2.constant) - source->op_array.literals);
#endif
				if (idx < cached->last_literal) {
					if (literal_map[idx] == 0) {
						literal_map[idx] = 1;
						new_last_literal++;
					}
				}
			}
			src_opline++;
		}
	}

	if (new_last_literal) {
		zval    *literal = source->op_array.literals;
		zval    *slot = php_parallel_cache_alloc(sizeof(zval) * new_last_literal);
		uint32_t idx = 0;

		offset_map = emalloc(sizeof(uint32_t) * cached->last_literal);

		cached->literals = slot;

		for (uint32_t i = 0; i < cached->last_literal; i++) {
			/* Record the mapping from old literal index (i) to new literal index (idx)
			   so we can update opcode operands later. */
			offset_map[i] = idx;

			if (Z_TYPE_P(literal) == IS_ARRAY) {
				ZVAL_ARR(slot, php_parallel_copy_hash_persistent(Z_ARRVAL_P(literal), php_parallel_copy_string_interned,
				                                                 php_parallel_cache_copy_mem,
				                                                 PHP_PARALLEL_COPY_STORAGE_CACHE_POOL));
			} else if (Z_TYPE_P(literal) == IS_STRING) {
				ZVAL_STR(slot, php_parallel_copy_string_interned(Z_STR_P(literal)));
			} else {
				*slot = *literal;
			}

			Z_TYPE_FLAGS_P(slot) &= ~(IS_TYPE_REFCOUNTED | IS_TYPE_COLLECTABLE);

			/* If this literal was used by INIT_FCALL, insert its lowercased version next. */
			if (literal_map[i]) {
				zend_string *lower = zend_string_tolower(Z_STR_P(slot));
				slot++;
				idx++;
				ZVAL_STR(slot, php_parallel_copy_string_interned(lower));
				zend_string_release(lower);
				Z_TYPE_FLAGS_P(slot) &= ~(IS_TYPE_REFCOUNTED | IS_TYPE_COLLECTABLE);
			}

			literal++;
			slot++;
			idx++;
		}
		cached->last_literal = new_last_literal;
	}

	if (cached->last_var) {
		zend_string **vars = cached->vars;
		uint32_t      it = 0, end = cached->last_var;
		zend_string **heap = php_parallel_cache_alloc(cached->last_var * sizeof(zend_string *));

		while (it < end) {
			heap[it] = php_parallel_copy_string_interned(vars[it]);
			it++;
		}
		cached->vars = heap;
	}

	if (cached->last) {
		zend_op *opcodes = php_parallel_cache_copy_mem(cached->opcodes, sizeof(zend_op) * cached->last);
		zend_op *opline = opcodes, *end = opline + cached->last;

		while (opline < end) {
			/* Replace ZEND_INIT_FCALL with ZEND_INIT_FCALL_BY_NAME.
			   We must clear op1_type (IS_UNUSED) and op1.var (0) to invalidate the
			   original thread's cache slot. */
			if (opline->opcode == ZEND_INIT_FCALL) {
				opline->opcode = ZEND_INIT_FCALL_BY_NAME;
				opline->op1_type = IS_UNUSED;
				opline->op1.var = 0;
				ZEND_VM_SET_OPCODE_HANDLER(opline);
			}

			/* Remap IS_CONST operands to their new locations in the expanded literal table
			   using the offset_map we built earlier. */
			if (opline->op1_type == IS_CONST) {
				uint32_t idx;
				zend_op *src_opline = source->op_array.opcodes + (opline - opcodes);
#if ZEND_USE_ABS_CONST_ADDR
				idx = (zval *)src_opline->op1.zv - source->op_array.literals;
				opline->op1.zv = &cached->literals[offset_map[idx]];
#else
				idx = ((zval *)((char *)src_opline + src_opline->op1.constant) - source->op_array.literals);
				opline->op1.constant = (char *)&cached->literals[offset_map[idx]] - (char *)opline;
#endif
				if (opline->opcode == ZEND_SEND_VAL || opline->opcode == ZEND_SEND_VAL_EX ||
				    opline->opcode == ZEND_QM_ASSIGN) {
					zend_vm_set_opcode_handler_ex(opline, 0, 0, 0);
				}
			}
			if (opline->op2_type == IS_CONST) {
				uint32_t idx;
				zend_op *src_opline = source->op_array.opcodes + (opline - opcodes);
#if ZEND_USE_ABS_CONST_ADDR
				idx = (zval *)src_opline->op2.zv - source->op_array.literals;
				opline->op2.zv = &cached->literals[offset_map[idx]];
#else
				idx = ((zval *)((char *)src_opline + src_opline->op2.constant) - source->op_array.literals);
				opline->op2.constant = (char *)&cached->literals[offset_map[idx]] - (char *)opline;
#endif
			}
#if ZEND_USE_ABS_JMP_ADDR
			switch (opline->opcode) {
			case ZEND_JMP:
			case ZEND_FAST_CALL:
				opline->op1.jmp_addr = &opcodes[opline->op1.jmp_addr - source->op_array.opcodes];
				break;
#if PHP_VERSION_ID < 80200
			case ZEND_JMPZNZ:
#endif
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

	if (literal_map) {
		efree(literal_map);
	}

	if (offset_map) {
		efree(offset_map);
	}

	if (cached->arg_info) {
		zend_arg_info *it = cached->arg_info, *end = it + cached->num_args, *info;

		if (cached->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
			it--;
		}
		if (cached->fn_flags & ZEND_ACC_VARIADIC) {
			end++;
		}

		cached->arg_info = info = php_parallel_cache_copy_mem(it, (end - it) * sizeof(zend_arg_info));

		while (it < end) {
			if (info->name) {
				info->name = php_parallel_copy_string_interned(it->name);
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
		cached->try_catch_array = php_parallel_cache_copy_mem(cached->try_catch_array,
		                                                      sizeof(zend_try_catch_element) * cached->last_try_catch);
	}

	if (cached->live_range) {
		cached->live_range =
		    php_parallel_cache_copy_mem(cached->live_range, sizeof(zend_live_range) * cached->last_live_range);
	}

	if (cached->function_name)
		cached->function_name = php_parallel_copy_string_interned(cached->function_name);

	if (cached->filename)
		cached->filename = php_parallel_copy_string_interned(cached->filename);

	if (cached->doc_comment)
		cached->doc_comment = php_parallel_copy_string_interned(cached->doc_comment);

_php_parallel_cached_function_return:
	return cached;
} /* }}} */

/* {{{ */
static zend_always_inline zend_function *
php_parallel_cache_function_ex(const zend_function *source PARALLEL_CACHE_STATICS_PARAM)
{
	php_parallel_cache_entry_t *entry;
	zend_op_array              *cached;
#if PHP_VERSION_ID >= 80400
	uint64_t source_hash = php_parallel_cache_file_hash(&source->op_array);
#endif

	pthread_mutex_lock(&PCG(mutex));

	if ((entry = zend_hash_index_find_ptr(&PCG(functions), (zend_ulong)source->op_array.opcodes))) {
#if PHP_VERSION_ID >= 80400
		if (!zend_string_equals(entry->function->function_name, source->op_array.function_name) ||
		    entry->source != source_hash) {
			goto _php_parallel_cached_function_create;
		}
#endif
		cached = entry->function;
		goto _php_parallel_cached_function_return;
	}

#if PHP_VERSION_ID >= 80400
_php_parallel_cached_function_create:
#endif
	entry = php_parallel_cache_alloc(sizeof(php_parallel_cache_entry_t));

	cached = php_parallel_cache_create(source PARALLEL_CACHE_STATICS_ARG(statics));
	entry->function = cached;
#if PHP_VERSION_ID >= 80400
	entry->source = source_hash;
#endif

	zend_hash_index_update_ptr(&PCG(functions), (zend_ulong)source->op_array.opcodes, entry);

_php_parallel_cached_function_return:
	pthread_mutex_unlock(&PCG(mutex));

	return (zend_function *)cached;
} /* }}} */

zend_function *php_parallel_cache_closure(const zend_function *source, zend_function *closure)
{ /* {{{ */
	zend_op_array *cache;

	cache = (zend_op_array *)php_parallel_cache_function_ex((zend_function *)source PARALLEL_CACHE_STATICS_ARG(0));

	if (!closure) {
		closure = php_parallel_copy_mem(cache, sizeof(zend_op_array), 1);
	} else {
		memcpy(closure, cache, sizeof(zend_op_array));
	}

	if (source->op_array.static_variables) {
		HashTable *statics = ZEND_MAP_PTR_GET(source->op_array.static_variables_ptr);

		if (statics) {
			closure->op_array.static_variables = php_parallel_copy_hash_ctor(statics, 1);

#if PHP_VERSION_ID >= 80200
			ZEND_MAP_PTR_INIT(closure->op_array.static_variables_ptr, closure->op_array.static_variables);
#else
			ZEND_MAP_PTR_INIT(closure->op_array.static_variables_ptr, &closure->op_array.static_variables);
#endif
		}
	}

#if PHP_VERSION_ID >= 80100
	if (source->op_array.num_dynamic_func_defs) {
		uint32_t it = 0;
		/* Use regular persistent memory for dynamic_func_defs array, not cache pool */
		closure->op_array.dynamic_func_defs =
		    pemalloc(sizeof(zend_op_array *) * source->op_array.num_dynamic_func_defs, 1);
		memcpy(closure->op_array.dynamic_func_defs, source->op_array.dynamic_func_defs,
		       sizeof(zend_op_array *) * source->op_array.num_dynamic_func_defs);
		while (it < source->op_array.num_dynamic_func_defs) {
			closure->op_array.dynamic_func_defs[it] = (zend_op_array *)php_parallel_cache_closure(
			    (zend_function *)source->op_array.dynamic_func_defs[it], NULL);
			it++;
		}
	}
#endif

	return closure;
} /* }}} */

#if PHP_VERSION_ID < 80200
zend_function *php_parallel_cache_function(const zend_function *source)
{ /* {{{ */
	return php_parallel_cache_function_ex(source, 1);
} /* }}} */
#endif

/* {{{ */
PHP_MINIT_FUNCTION(PARALLEL_CACHE)
{
	zend_hash_init(&PCG(functions), 32, NULL, NULL, 1);
#if PHP_VERSION_ID < 80200
	zend_hash_init(&PCG(statics), 32, NULL, NULL, 1);
#endif

	PCM(size) = PARALLEL_CACHE_CHUNK;
	PCM(mem) = PCM(block) = malloc(PCM(size));

	if (!PCM(mem)) {
		/* out of memory */
	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_CACHE)
{
	zend_hash_destroy(&PCG(functions));
#if PHP_VERSION_ID < 80200
	zend_hash_destroy(&PCG(statics));
#endif

	if (PCM(mem))
		free(PCM(mem));

	return SUCCESS;
} /* }}} */
