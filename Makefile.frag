fmt:
	find src \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i -style=file

format: fmt
