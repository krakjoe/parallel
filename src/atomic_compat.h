/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Florian Engelhardt 2026                                  |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Florian Engelhardt                                           |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PARALLEL_ATOMIC_COMPAT_H
#define HAVE_PARALLEL_ATOMIC_COMPAT_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__) && defined(__has_include)
#if __has_include(<stdatomic.h>)
#define PHP_PARALLEL_HAVE_STDATOMIC 1
#endif
#endif

#ifdef PHP_PARALLEL_HAVE_STDATOMIC
#include <stdatomic.h>
#else

// shortcut: only atomic_uint is emulated; remove when requiring PHP 8.7.
typedef struct _php_parallel_atomic_uint {
#ifdef _WIN32
	volatile LONG value;
#else
	unsigned int value;
#endif
} atomic_uint;

#define atomic_init php_parallel_atomic_init
#define atomic_load php_parallel_atomic_load
#define atomic_fetch_add php_parallel_atomic_fetch_add
#define atomic_fetch_sub php_parallel_atomic_fetch_sub

static zend_always_inline void php_parallel_atomic_init(atomic_uint *obj, unsigned int desired)
{
	obj->value = desired;
}

static zend_always_inline unsigned int php_parallel_atomic_load(atomic_uint *obj)
{
#ifdef _WIN32
	return (unsigned int)InterlockedCompareExchange(&obj->value, 0, 0);
#elif defined(HAVE_BUILTIN_ATOMIC_CPP11)
	return __atomic_load_n(&obj->value, __ATOMIC_SEQ_CST);
#else
	return __sync_fetch_and_add(&obj->value, 0);
#endif
}

static zend_always_inline unsigned int php_parallel_atomic_fetch_add(atomic_uint *obj, unsigned int operand)
{
#ifdef _WIN32
	return (unsigned int)InterlockedExchangeAdd(&obj->value, (LONG)operand);
#elif defined(HAVE_BUILTIN_ATOMIC_CPP11)
	return __atomic_fetch_add(&obj->value, operand, __ATOMIC_SEQ_CST);
#else
	return __sync_fetch_and_add(&obj->value, operand);
#endif
}

static zend_always_inline unsigned int php_parallel_atomic_fetch_sub(atomic_uint *obj, unsigned int operand)
{
	return php_parallel_atomic_fetch_add(obj, 0u - operand);
}

#endif
#endif
