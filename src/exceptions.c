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
#ifndef HAVE_PARALLEL_EXCEPTIONS
#define HAVE_PARALLEL_EXCEPTIONS

#include "parallel.h"

zend_class_entry* php_parallel_error_ce;
zend_class_entry* php_parallel_error_invalid_arguments_ce;

zend_class_entry* php_parallel_runtime_error_ce;
zend_class_entry* php_parallel_runtime_error_bootstrap_ce;
zend_class_entry* php_parallel_runtime_error_closed_ce;
zend_class_entry* php_parallel_runtime_error_killed_ce;
zend_class_entry* php_parallel_runtime_error_illegal_function_ce;
zend_class_entry* php_parallel_runtime_error_illegal_instruction_ce;
zend_class_entry* php_parallel_runtime_error_illegal_variable_ce;
zend_class_entry* php_parallel_runtime_error_illegal_parameter_ce;
zend_class_entry* php_parallel_runtime_error_illegal_type_ce;
zend_class_entry* php_parallel_runtime_error_illegal_return_ce;

zend_class_entry* php_parallel_future_error_ce;
zend_class_entry* php_parallel_future_error_killed_ce;
zend_class_entry* php_parallel_future_error_uncaught_ce;

zend_class_entry* php_parallel_channel_error_ce;
zend_class_entry* php_parallel_channel_error_existence_ce;
zend_class_entry* php_parallel_channel_error_illegal_value_ce;
zend_class_entry* php_parallel_channel_error_closed_ce;

zend_class_entry* php_parallel_events_error_ce;
zend_class_entry* php_parallel_events_error_existence_ce;
zend_class_entry* php_parallel_events_error_timeout_ce;

zend_class_entry* php_parallel_events_input_error_ce;
zend_class_entry* php_parallel_events_input_error_existence_ce;

zend_class_entry* php_parallel_events_event_error_ce;

void php_parallel_exceptions_startup() {
    zend_class_entry ce;

    /*
    * Base Exceptions
    */
	INIT_NS_CLASS_ENTRY(ce, "parallel", "Error", NULL);
	php_parallel_error_ce = 
	    zend_register_internal_class_ex(&ce, zend_ce_error_exception);

	INIT_NS_CLASS_ENTRY(ce, "parallel\\Error", "InvalidArguments", NULL);
	php_parallel_error_invalid_arguments_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	
	/*
	* Runtime Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime", "Error", NULL);
	php_parallel_runtime_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Bootstrap", NULL);
	php_parallel_runtime_error_bootstrap_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Closed", NULL);
	php_parallel_runtime_error_closed_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "Killed", NULL);
	php_parallel_runtime_error_killed_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);

	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalFunction", NULL);
	php_parallel_runtime_error_illegal_function_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalParameter", NULL);
	php_parallel_runtime_error_illegal_parameter_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
 
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalInstruction", NULL);
	php_parallel_runtime_error_illegal_instruction_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Runtime\\Error", "IllegalReturn", NULL);
	php_parallel_runtime_error_illegal_return_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_runtime_error_ce);
	    
	/*
	* Future Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Future", "Error", NULL);
	php_parallel_future_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Future\\Error", "Killed", NULL);
	php_parallel_future_error_killed_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Future\\Error", "Uncaught", NULL);
	php_parallel_future_error_uncaught_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	/*
	* Channel Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel", "Error", NULL);
	php_parallel_channel_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "Existence", NULL);
	php_parallel_channel_error_existence_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);	
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "IllegalValue", NULL);
	php_parallel_channel_error_illegal_value_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Channel\\Error", "Closed", NULL);
	php_parallel_channel_error_closed_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_channel_error_ce);
	    
	/*
	* Events Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events", "Error", NULL);
	php_parallel_events_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Error", "Existence", NULL);
	php_parallel_events_error_existence_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_events_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Error", "Timeout", NULL);
	php_parallel_events_error_timeout_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_events_error_ce);	
	    
	/*
	* Input Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Input", "Error", NULL);
	php_parallel_events_input_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);
	    
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Input\\Error", "Existence", NULL);
	php_parallel_events_input_error_existence_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_events_input_error_ce);
	    
	/*
	* Event Exceptions
	*/
	INIT_NS_CLASS_ENTRY(ce, "parallel\\Events\\Event", "Error", NULL);
	php_parallel_events_event_error_ce = 
	    zend_register_internal_class_ex(&ce, php_parallel_error_ce);	
}
#endif
