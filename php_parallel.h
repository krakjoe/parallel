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

#ifndef PHP_PARALLEL_H
# define PHP_PARALLEL_H

#include "php.h"

#include "ext/standard/info.h"
#include "php_ini.h"
#include "pthread.h"

extern zend_module_entry parallel_module_entry;
# define phpext_parallel_ptr &parallel_module_entry

# define PHP_PARALLEL_VERSION "1.3.0-dev"

typedef struct _php_parallel_output_t {
	pthread_mutex_t mutex;
	uint32_t        workers; // Created threads not yet joined, including nested runtimes.
} php_parallel_output_t;

ZEND_BEGIN_MODULE_GLOBALS(parallel)
php_parallel_output_t  output_context;
php_parallel_output_t *output;
ZEND_END_MODULE_GLOBALS(parallel)

ZEND_EXTERN_MODULE_GLOBALS(parallel)

#define PARALLEL_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(parallel, v)

# if !defined(ZTS)
# error Only ZTS build are supported
# elif defined(COMPILE_DL_PARALLEL)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#if _WIN32
# define PARALLEL_API __declspec(dllexport)
#else
# define PARALLEL_API __attribute__ ((visibility("default")))
#endif

PARALLEL_API bool php_parallel_is_parallel_worker_thread(void);

#endif	/* PHP_PARALLEL_H */
