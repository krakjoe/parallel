/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019-2022                                  |
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
#ifndef HAVE_PARALLEL_PARALLEL_H
#define HAVE_PARALLEL_PARALLEL_H

#include "php.h"
#include "monitor.h"
#include "pthread.h"

#include "exceptions.h"
#include "handlers.h"
#include "runtime.h"
#include "future.h"
#include "scheduler.h"
#include "channel.h"
#include "events.h"
#include "event.h"
#include "input.h"
#include "sync.h"

#include "SAPI.h"
#include "php_main.h"
#include "zend_closures.h"
#include "zend_gc.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "zend_vm.h"

typedef union _php_parallel_platform_align_test {
    void *v;
    double d;
    zend_long l;
} php_parallel_platform_align_test;

#if ZEND_GCC_VERSION >= 2000
# define PARALLEL_PLATFORM_ALIGNMENT \
    (__alignof__(php_parallel_platform_align_test) < 8 ? \
        8 : __alignof__(php_parallel_platform_align_test))
#else
# define PARALLEL_PLATFORM_ALIGNMENT \
    (sizeof(php_parallel_platform_align_test))
#endif
#define PARALLEL_PLATFORM_ALIGNED(size) \
    ZEND_MM_ALIGNED_SIZE_EX(size, PARALLEL_PLATFORM_ALIGNMENT)

#include "dependencies.h"
#include "cache.h"
#include "copy.h"
#include "check.h"

#define PARALLEL_PARAMETERS_NONE(r) \
    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 0, 0) \
    ZEND_PARSE_PARAMETERS_END()

static zend_always_inline zend_bool php_parallel_mutex_init(pthread_mutex_t *mutex, zend_bool recursive) {
    if (recursive) {
        zend_bool result = 0;
        pthread_mutexattr_t attributes;

        pthread_mutexattr_init(&attributes);
#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
        pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
        pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif
        if (pthread_mutex_init(mutex, &attributes) == SUCCESS) {
            result = 1;
        }

        pthread_mutexattr_destroy(&attributes);
        return result;
    }

    return (pthread_mutex_init(mutex, NULL) == SUCCESS);
}

static zend_always_inline void php_parallel_mutex_destroy(pthread_mutex_t *mutex) {
    pthread_mutex_destroy(mutex);
}

static zend_always_inline zend_bool php_parallel_cond_init(pthread_cond_t *cond) {
    return (pthread_cond_init(cond, NULL) == SUCCESS);
}

static zend_always_inline void php_parallel_cond_destroy(pthread_cond_t *cond) {
    pthread_cond_destroy(cond);
}

static zend_always_inline void php_parallel_atomic_addref(uint32_t *refcount) {
#ifdef _WIN32
    InterlockedAdd(refcount, 1);
#elif defined(HAVE_BUILTIN_ATOMIC_CPP11)
    __atomic_add_fetch(refcount, 1, __ATOMIC_SEQ_CST);
#else
    __sync_add_and_fetch(refcount, 1);
#endif
}

static zend_always_inline uint32_t php_parallel_atomic_delref(uint32_t *refcount) {
#ifdef _WIN32
    return InterlockedAdd(refcount, -1);
#elif defined(HAVE_BUILTIN_ATOMIC_CPP11)
    return __atomic_sub_fetch(refcount, 1, __ATOMIC_SEQ_CST);
#else
    return __sync_sub_and_fetch(refcount, 1);
#endif
}

extern zend_function_entry php_parallel_functions[];

PHP_MINIT_FUNCTION(PARALLEL_CORE);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_CORE);

PHP_RINIT_FUNCTION(PARALLEL_CORE);
PHP_RSHUTDOWN_FUNCTION(PARALLEL_CORE);

ZEND_BEGIN_ARG_INFO(php_parallel_no_args_arginfo, 0)
ZEND_END_ARG_INFO()
#endif
