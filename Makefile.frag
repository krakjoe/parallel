parallel-test-coverage:
	CCACHE_DISABLE=1 EXTRA_CFLAGS="-fprofile-arcs -ftest-coverage" TEST_PHP_ARGS="-q" $(MAKE) clean test

parallel-test-coverage-lcov: parallel-test-coverage
	lcov -c --directory $(top_srcdir)/src/.libs --output-file $(top_srcdir)/coverage.info

parallel-test-coverage-html: parallel-test-coverage-lcov
	genhtml $(top_srcdir)/coverage.info --output-directory=$(top_srcdir)/html

parallel-test-coverage-travis:
	CCACHE_DISABLE=1 EXTRA_CFLAGS="-fprofile-arcs -ftest-coverage" $(MAKE)
