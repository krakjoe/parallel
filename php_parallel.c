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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_parallel.h"

#include "src/parallel.h"

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(parallel)
{
	php_parallel_startup();

	return SUCCESS;
} /* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(parallel)
{
	php_parallel_shutdown();

	return SUCCESS;
} /* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(parallel)
{
#if defined(ZTS) && defined(COMPILE_DL_PARALLEL)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

    php_parallel_copy_startup();
    php_parallel_check_startup();
    php_parallel_strings_startup();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(parallel)
{
    if (!CG(unclean_shutdown)) {
        php_parallel_strings_shutdown();
        php_parallel_check_shutdown();
        php_parallel_copy_shutdown();
    }

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(parallel)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "parallel support", "enabled");
	php_info_print_table_row(2, "parallel version", PHP_PARALLEL_VERSION);
	php_info_print_table_end();
}
/* }}} */

/* {{{ parallel_module_entry
 */
zend_module_entry parallel_module_entry = {
	STANDARD_MODULE_HEADER,
	"parallel",
	NULL,
	PHP_MINIT(parallel),
	PHP_MSHUTDOWN(parallel),
	PHP_RINIT(parallel),
	PHP_RSHUTDOWN(parallel),
	PHP_MINFO(parallel),
	PHP_PARALLEL_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PARALLEL
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(parallel)
#endif

