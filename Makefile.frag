fmt:
	find src \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i -style=file

format: fmt

.PHONY: analyze
analyze:
	@if $(CC) -dM -E - </dev/null | grep -q __clang__; then \
		flags='--analyze -Xanalyzer -analyzer-werror -Xanalyzer -analyzer-output=text'; \
	else \
		flags='-fanalyzer -Werror -c'; \
	fi; \
	status=0; \
	for src in $(shared_objects_parallel:.lo=.c); do \
		$(CC) $$flags -I. $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -Wall -O0 -fPIC \
			-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -DZEND_COMPILE_DL_EXT=1 \
			"$(srcdir)/$$src" -o /dev/null || status=1; \
	done > analyze.log 2>&1 || exit 1; \
	cat analyze.log; exit $$status
