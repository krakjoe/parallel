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
#ifndef HAVE_PARALLEL_EXCEPTIONS_H
#define HAVE_PARALLEL_EXCEPTIONS_H

#define php_parallel_exception_ex(type, m, ...)      zend_throw_exception_ex(type, 0, m, ##__VA_ARGS__)
#define php_parallel_exception(m, ...)               php_parallel_exception_ex(php_parallel_error_ce, m, ##__VA_ARGS__)
#define php_parallel_invalid_arguments(m, ...)       php_parallel_exception_ex(zend_ce_type_error, m, ##__VA_ARGS__)

/*
* Base Exception
*/
extern zend_class_entry* php_parallel_error_ce;


/*
* Runtime Exceptions
*/
extern zend_class_entry* php_parallel_runtime_error_ce;
extern zend_class_entry* php_parallel_runtime_error_bootstrap_ce;
extern zend_class_entry* php_parallel_runtime_error_closed_ce;
extern zend_class_entry* php_parallel_runtime_error_killed_ce;
extern zend_class_entry* php_parallel_runtime_error_illegal_function_ce;
extern zend_class_entry* php_parallel_runtime_error_illegal_instruction_ce;
extern zend_class_entry* php_parallel_runtime_error_illegal_variable_ce;
extern zend_class_entry* php_parallel_runtime_error_illegal_parameter_ce;
extern zend_class_entry* php_parallel_runtime_error_illegal_return_ce;

/*
* Future Exceptions
*/
extern zend_class_entry* php_parallel_future_error_ce;
extern zend_class_entry* php_parallel_future_error_killed_ce;
extern zend_class_entry* php_parallel_future_error_cancelled_ce;
extern zend_class_entry* php_parallel_future_error_foreign_ce;

/*
* Channel Exceptions
*/
extern zend_class_entry* php_parallel_channel_error_ce;
extern zend_class_entry* php_parallel_channel_error_existence_ce;
extern zend_class_entry* php_parallel_channel_error_illegal_value_ce;
extern zend_class_entry* php_parallel_channel_error_closed_ce;

/*
* Sync Exceptions
*/
extern zend_class_entry* php_parallel_sync_error_ce;
extern zend_class_entry* php_parallel_sync_error_illegal_value_ce;

/*
* Events Exceptions
*/
extern zend_class_entry* php_parallel_events_error_ce;
extern zend_class_entry* php_parallel_events_error_existence_ce;
extern zend_class_entry* php_parallel_events_error_timeout_ce;

/*
* Input Exceptions
*/
extern zend_class_entry* php_parallel_events_input_error_ce;
extern zend_class_entry* php_parallel_events_input_error_existence_ce;
extern zend_class_entry* php_parallel_events_input_error_illegal_value_ce;

/*
* Event Exceptions
*/
extern zend_class_entry* php_parallel_events_event_error_ce;

typedef struct _php_parallel_exception_t php_parallel_exception_t;

void         php_parallel_exceptions_save(zval *saved, zend_object *exception);
zend_object* php_parallel_exceptions_restore(zval *exception);
void         php_parallel_exceptions_destroy(php_parallel_exception_t *ex);

PHP_MINIT_FUNCTION(PARALLEL_EXCEPTIONS);
PHP_MSHUTDOWN_FUNCTION(PARALLEL_EXCEPTIONS);
#endif
