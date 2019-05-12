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
#ifndef HAVE_PARALLEL_PARALLEL
#define HAVE_PARALLEL_PARALLEL

#include "parallel.h"

typedef int (*php_sapi_deactivate_t)(void);
typedef size_t (*php_sapi_output_t)(const char*, size_t);

static php_sapi_deactivate_t php_sapi_deactivate_function;
static php_sapi_output_t     php_sapi_output_function;

static pthread_mutex_t php_parallel_output_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t php_parallel_output_function(const char *str, size_t len) {
    size_t result;

    pthread_mutex_lock(&php_parallel_output_mutex);
    result =
        php_sapi_output_function(str, len);
    pthread_mutex_unlock(&php_parallel_output_mutex);

    return result;
}

PHP_MINIT_FUNCTION(PARALLEL_CORE)
{
    if (strncmp(sapi_module.name, "cli", sizeof("cli")-1) == SUCCESS) {
        php_sapi_deactivate_function = sapi_module.deactivate;

        sapi_module.deactivate = NULL;
    }

    php_sapi_output_function = sapi_module.ub_write;

    sapi_module.ub_write = php_parallel_output_function;

    PHP_MINIT(PARALLEL_HANDLERS)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_EXCEPTIONS)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_COPY)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_SCHEDULER)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_CHANNEL)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PARALLEL_EVENTS)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_CORE)
{
    PHP_MSHUTDOWN(PARALLEL_EVENTS)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_CHANNEL)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_SCHEDULER)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_COPY)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_EXCEPTIONS)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PARALLEL_HANDLERS)(INIT_FUNC_ARGS_PASSTHRU);

    if (strncmp(sapi_module.name, "cli", sizeof("cli")-1) == SUCCESS) {
        sapi_module.deactivate = php_sapi_deactivate_function;
    }

    sapi_module.ub_write = php_sapi_output_function;

    return SUCCESS;
}

PHP_RINIT_FUNCTION(PARALLEL_CORE)
{
    PHP_RINIT(PARALLEL_COPY)(INIT_FUNC_ARGS_PASSTHRU);

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(PARALLEL_CORE)
{
    if (!CG(unclean_shutdown)) {
        PHP_RSHUTDOWN(PARALLEL_COPY)(INIT_FUNC_ARGS_PASSTHRU);
    }

	return SUCCESS;
}
#endif
