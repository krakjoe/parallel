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

zend_class_entry *php_parallel_exception_ce;

static pthread_mutex_t php_parallel_output_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t php_parallel_output_function(const char *str, size_t len) {
    size_t result;
    
    pthread_mutex_lock(&php_parallel_output_mutex);
    result = 
        php_sapi_output_function(str, len);
    pthread_mutex_unlock(&php_parallel_output_mutex);
    
    return result;
}

void php_parallel_startup(void) {
	if (strncmp(sapi_module.name, "cli", sizeof("cli")-1) == SUCCESS) {
		php_sapi_deactivate_function = sapi_module.deactivate;
        
		sapi_module.deactivate = NULL;
	}
	
    php_sapi_output_function = sapi_module.ub_write;
    
    sapi_module.ub_write = php_parallel_output_function;
    
    php_parallel_exceptions_startup();
    php_parallel_handlers_startup();
	php_parallel_scheduler_startup();
	php_parallel_runtime_startup();
	php_parallel_future_startup();
	php_parallel_channel_startup();
	php_parallel_events_startup();
}

void php_parallel_shutdown(void) {
	php_parallel_channel_shutdown();
	php_parallel_scheduler_shutdown();
	php_parallel_runtime_shutdown();
	php_parallel_events_shutdown();
	
	if (strncmp(sapi_module.name, "cli", sizeof("cli")-1) == SUCCESS) {
		sapi_module.deactivate = php_sapi_deactivate_function;
	}
	
	sapi_module.ub_write = php_sapi_output_function;
}
#endif
