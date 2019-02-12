dnl config.m4 for extension parallel

PHP_ARG_ENABLE(parallel, whether to enable parallel support,
[  --enable-parallel          Enable parallel support], no)

PHP_ARG_ENABLE(parallel-coverage,      whether to enable parallel coverage support,
[  --enable-parallel-coverage          Enable parallel coverage support], no, no)


if test "$PHP_PARALLEL" != "no"; then
  AC_DEFINE(HAVE_PARALLEL, 1, [ Have parallel support ])

  PHP_NEW_EXTENSION(parallel, php_parallel.c src/monitor.c src/parallel.c src/copy.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  PHP_ADD_BUILD_DIR($ext_builddir/src, 1)
  PHP_ADD_INCLUDE($ext_builddir)

  AC_MSG_CHECKING([parallel coverage])
  if test "$PHP_PARALLEL_COVERAGE" != "no"; then
    AC_MSG_RESULT([enabled])

    PHP_ADD_MAKEFILE_FRAGMENT
  else
    AC_MSG_RESULT([disabled])
  fi

fi
