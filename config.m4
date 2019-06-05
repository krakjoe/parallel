dnl config.m4 for extension parallel

PHP_ARG_ENABLE(parallel, whether to enable parallel support,
[  --enable-parallel          Enable parallel support], no)

PHP_ARG_ENABLE(parallel-coverage,      whether to enable parallel coverage support,
[  --enable-parallel-coverage Enable parallel coverage support], no, no)

PHP_ARG_ENABLE(parallel-dev, whether to enable parallel developer build flags,
[  --enable-parallel-dev      Enable parallel developer flags], no, no)

if test "$PHP_PARALLEL" != "no"; then

  AC_MSG_CHECKING([for ZTS])
  if test "$PHP_THREAD_SAFETY" != "no"; then
    AC_MSG_RESULT([ok])
  else
    AC_MSG_ERROR([parallel requires ZTS, please use PHP with ZTS enabled])
  fi

  AC_DEFINE(HAVE_PARALLEL, 1, [ Have parallel support ])

  AC_MSG_CHECKING([support for __atomic_add_fetch])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([], [[
    int variable = 1;
    return (__atomic_add_fetch(&variable, 1, __ATOMIC_SEQ_CST)) ? 1 : 0;
  ]])], [
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_BUILTIN_ATOMIC, 1, [Define to 1 if supports __atomic_add_fetch()])
  ], [
    AC_MSG_RESULT([no])
  ])

  if test "$PHP_PARALLEL_DEV" != "no"; then
    AX_CHECK_COMPILE_FLAG(-Werror,                  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Werror")
    AX_CHECK_COMPILE_FLAG(-Werror=format-security,  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Werror=format-security")
    AX_CHECK_COMPILE_FLAG(-Wno-unused-parameter,    _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-unused-parameter")
    AX_CHECK_COMPILE_FLAG(-fstack-protector,        _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fstack-protector")
    AX_CHECK_COMPILE_FLAG(-fstack-protector-strong, _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fstack-protector-strong")
  fi

  PHP_NEW_EXTENSION(parallel, php_parallel.c src/exceptions.c src/monitor.c src/parallel.c src/runtime.c src/scheduler.c src/future.c src/copy.c src/check.c src/dependencies.c src/cache.c src/channel.c src/link.c src/handlers.c src/events.c src/poll.c src/loop.c src/event.c src/input.c src/sync.c, $ext_shared,, "-Wall -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 $_MAINTAINER_CFLAGS")

  PHP_ADD_BUILD_DIR($ext_builddir/src, 1)
  PHP_ADD_INCLUDE($ext_srcdir)

  AC_MSG_CHECKING([parallel coverage])
  if test "$PHP_PARALLEL_COVERAGE" != "no"; then
    AC_MSG_RESULT([enabled])

    PHP_ADD_MAKEFILE_FRAGMENT
  else
    AC_MSG_RESULT([disabled])
  fi

fi
