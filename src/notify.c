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
#ifndef HAVE_PARALLEL_NOTIFY
#define HAVE_PARALLEL_NOTIFY

#include "notify.h"

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

void php_parallel_notify_init(php_parallel_notify_t *notify)
{
	notify->read = -1;
	notify->write = -1;
	notify->raised = false;
}

#ifndef _WIN32
static bool php_parallel_notify_flags(int descriptor)
{
	int flags = fcntl(descriptor, F_GETFL);

	if (flags == -1 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == -1) {
		return false;
	}

	flags = fcntl(descriptor, F_GETFD);

	return flags != -1 && fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) != -1;
}

static bool php_parallel_notify_create(php_parallel_notify_t *notify)
{
	int descriptors[2];

	if (pipe(descriptors) == -1) {
		return false;
	}

	if (!php_parallel_notify_flags(descriptors[0]) || !php_parallel_notify_flags(descriptors[1])) {
		close(descriptors[0]);
		close(descriptors[1]);
		return false;
	}

	notify->read = descriptors[0];
	notify->write = descriptors[1];

	return true;
}

#endif

int php_parallel_notify_observe(php_parallel_notify_t *notify, bool ready)
{
#ifdef _WIN32
	return -1;
#else
	if (notify->read == -1 && !php_parallel_notify_create(notify)) {
		return -1;
	}

	php_parallel_notify_sync(notify, ready);

	return notify->read;
#endif
}

void php_parallel_notify_sync(php_parallel_notify_t *notify, bool ready)
{
#ifndef _WIN32
	char    byte = 0;
	ssize_t result;

	if (notify->read == -1 || notify->raised == ready) {
		return;
	}

	do {
		result = ready ? write(notify->write, &byte, sizeof(byte)) : read(notify->read, &byte, sizeof(byte));
	} while (result == -1 && errno == EINTR);

	if (result == sizeof(byte) || (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
		notify->raised = ready;
	}
#endif
}

void php_parallel_notify_destroy(php_parallel_notify_t *notify)
{
#ifndef _WIN32
	if (notify->read != -1) {
		close(notify->read);
		close(notify->write);
	}
#endif
}
#endif
