/*
  +----------------------------------------------------------------------+
  | parallel                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Florian Engelhardt 2026                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Florian Engelhardt <flo@dotbox.org>                          |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PARALLEL_NOTIFY_H
#define HAVE_PARALLEL_NOTIFY_H

#include <stdbool.h>

typedef struct _php_parallel_notify_t {
	int  read;
	int  write;
	bool raised;
} php_parallel_notify_t;

void php_parallel_notify_init(php_parallel_notify_t *notify);
int  php_parallel_notify_observe(php_parallel_notify_t *notify, bool ready);
void php_parallel_notify_sync(php_parallel_notify_t *notify, bool ready);
void php_parallel_notify_destroy(php_parallel_notify_t *notify);

#endif
