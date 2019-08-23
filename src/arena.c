/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
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
#ifndef PHP_PARALLEL_ARENA
# define PHP_PARALLEL_ARENA

#include "parallel.h"

#ifndef PHP_PARALLEL_ARENA_DEBUG
# define PHP_PARALLEL_ARENA_DEBUG 0
#endif

#if !defined(_WIN32)
# include <sys/mman.h>
#endif

static zend_always_inline void* php_parallel_map(zend_long size) {
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *mapped = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    if (EXPECTED(mapped != MAP_FAILED)) {
        return mapped;
    }

    return NULL;
#endif
}

static zend_always_inline void php_parallel_unmap(void *address, zend_long size) {
#if defined(_WIN32)
    VirtualFree(address, 0, MEM_RELEASE);
#else
    if (UNEXPECTED(NULL == address)) {
        return;
    }

    munmap(address, size);
#endif
}

typedef struct _php_parallel_arena_block_t php_parallel_arena_block_t;

typedef intptr_t php_parallel_arena_ptr_t;

struct _php_parallel_arena_block_t {
    zend_long                    size;
    zend_long                    used;
    php_parallel_arena_block_t     *next;
    php_parallel_arena_ptr_t        mem[1];
};

#define PHP_PARALLEL_ARENA_BLOCK_SIZE \
    php_parallel_arena_aligned(sizeof(php_parallel_arena_block_t))
#define PHP_PARALLEL_ARENA_BLOCK_MIN \
    (PHP_PARALLEL_ARENA_BLOCK_SIZE * 2)

struct _php_parallel_arena_t {
    pthread_mutex_t               mutex;
    zend_long                     size;
    zend_ulong                    bytes;
    char                         *brk;
    char                         *end;
    struct {
        php_parallel_arena_block_t  *start;
        php_parallel_arena_block_t  *end;
    } list;
};

#define PHP_PARALLEL_ARENA_SIZE \
    php_parallel_arena_aligned(sizeof(php_parallel_arena_t))
#define PHP_PARALLEL_ARENA_OOM \
    (void*) -1

static zend_always_inline zend_long php_parallel_arena_aligned(zend_long size) {
    return (size + sizeof(php_parallel_arena_ptr_t) - 1) & ~(sizeof(php_parallel_arena_ptr_t) - 1);
}

static zend_always_inline php_parallel_arena_block_t* php_parallel_arena_block(void *mem) {
    return (php_parallel_arena_block_t*) (((char*) mem) - XtOffsetOf(php_parallel_arena_block_t, mem));
}

static zend_always_inline php_parallel_arena_block_t* php_parallel_arena_find(php_parallel_arena_t *arena, zend_long size) {
    php_parallel_arena_block_t *block = arena->list.start;
    zend_long wasted;

    while (NULL != block) {
        if ((0 == block->used)) {
            if ((block->size >= size)) {
                goto _php_parallel_arena_found;
            } else {
                if (NULL != block->next) {
                    if ((0 == block->next->used) &&
                        ((block->size + block->next->size) >= size)) {
                        block->size += block->next->size;
                        block->next = block->next->next;
                        goto _php_parallel_arena_found;
                    }
                }
            }
        }

        block = block->next;
    }

    return NULL;

_php_parallel_arena_found:
    if (((wasted = (block->size - size)) > 0)) {
        if ((wasted > PHP_PARALLEL_ARENA_BLOCK_MIN)) {
            php_parallel_arena_block_t *reclaim =
                (php_parallel_arena_block_t*)
                    (((char*) block->mem) + size);

            reclaim->used = 0;
            reclaim->size =
                (wasted - PHP_PARALLEL_ARENA_BLOCK_SIZE);
            reclaim->next = block->next;

            block->next  = reclaim;
        }

        block->size = size;
    }
    block->used = size;

    return block;
}

php_parallel_arena_t* php_parallel_arena_create(zend_long size) {
    zend_long aligned =
        php_parallel_arena_aligned(PHP_PARALLEL_ARENA_SIZE + size);
    php_parallel_arena_t *arena =
        (php_parallel_arena_t*)
            php_parallel_map(aligned);

    if (!arena) {
        return NULL;
    }

    if (!php_parallel_mutex_init(&arena->mutex, 1)) {
        php_parallel_unmap(arena, aligned);
        return NULL;
    }

    arena->end = (char*) (((char*) arena) + aligned);
    arena->brk = (char*) (((char*) arena) + PHP_PARALLEL_ARENA_SIZE);
    arena->bytes = arena->end - arena->brk;
    arena->size = aligned;

    return arena;
}

static php_parallel_arena_block_t* php_parallel_arena_brk(php_parallel_arena_t *arena, zend_long size) {
    if (brk > 0) {
        size =
            php_parallel_arena_aligned(
                PHP_PARALLEL_ARENA_BLOCK_SIZE + size);

        if (UNEXPECTED((arena->brk + size) > arena->end)) {
            return PHP_PARALLEL_ARENA_OOM;
        }

        arena->brk += size;
    }

    return (php_parallel_arena_block_t*) arena->brk;
}

void* php_parallel_arena_alloc(php_parallel_arena_t *arena, zend_long size) {
    zend_long aligned =
        php_parallel_arena_aligned(size);
    php_parallel_arena_block_t *block;

    if (UNEXPECTED(SUCCESS !=
            pthread_mutex_lock(&arena->mutex))) {
        return NULL;
    }

    block = php_parallel_arena_find(arena, aligned);

    if (EXPECTED(NULL != block)) {
        goto _php_parallel_arena_alloc_leave;
    }

    block = php_parallel_arena_brk(arena, 0);

    if (UNEXPECTED(php_parallel_arena_brk(
            arena, aligned) == PHP_PARALLEL_ARENA_OOM)) {
        goto _php_parallel_arena_alloc_oom;
    }

    block->used =
        block->size = aligned;

    if (UNEXPECTED(NULL == arena->list.start)) {
        arena->list.start = block;
    }

    if (EXPECTED(NULL != arena->list.end)) {
        arena->list.end->next = block;
    }

    arena->list.end = block;

_php_parallel_arena_alloc_leave:
    pthread_mutex_unlock(&arena->mutex);

    return memset(block->mem, 0, block->size);

_php_parallel_arena_alloc_oom:
    pthread_mutex_unlock(&arena->mutex);

    return NULL;
}

#if PHP_PARALLEL_ARENA_DEBUG
static zend_always_inline zend_bool php_parallel_arena_owns(php_parallel_arena_t *arena, void *mem) {
    if (UNEXPECTED((((char*) mem) < ((char*) arena)) || (((char*) mem) > arena->end))) {
        return 0;
    }

    return 1;
}
#endif

void php_parallel_arena_free(php_parallel_arena_t *arena, void *mem) {
    php_parallel_arena_block_t *block = php_parallel_arena_block(mem);

#if PHP_PARALLEL_ARENA_DEBUG
    ZEND_ASSERT(php_parallel_arena_owns(arena, mem));
#endif

    pthread_mutex_lock(&arena->mutex);

    while ((NULL != block->next)) {
        if ((0 == block->next->used)) {
            if ((NULL != arena->list.end) &&
                (block->next == arena->list.end)) {
                arena->list.end = block->next->next;
            }

            block->size += block->next->size;
            block->next = block->next->next;
        } else {
            break;
        }
    }

    block->used = 0;

    pthread_mutex_unlock(&arena->mutex);
}

#if PHP_PARALLEL_ARENA_DEBUG
static zend_always_inline void php_parallel_arena_debug(php_parallel_arena_t *arena) {
    php_parallel_arena_block_t *block = arena->list.start;
    zend_long wasted = 0;

    while (NULL != block) {
        if (block->used > 0) {
            wasted += block->size - block->used;
        }

        if (block->used > 0) {
            fprintf(stderr,
                "[PARALLEL] %p leaked %p "ZEND_LONG_FMT" bytes\n",
                arena, block->mem, block->size);
        }
        block = block->next;
    }

    fprintf(stderr, "[PARALLEL] %p wasted "ZEND_LONG_FMT" bytes\n", arena, wasted);
}
#endif

void php_parallel_arena_destroy(php_parallel_arena_t *arena) {
    if (!arena) {
        return;
    }

#if PHP_PARALLEL_ARENA_DEBUG
    php_parallel_arena_debug(arena);
#endif

    php_parallel_mutex_destroy(&arena->mutex);

    php_parallel_unmap(arena, arena->size);
}
#endif
