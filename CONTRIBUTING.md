Contributing
============

To ease reproduction of test cases, development, and CI, parallel has adopted docker (with composition) as a core part of the development workflow.

parallel requires a ZTS build of PHP which is built as a separate environment upon which the testing environment relies.

Source Conventions
==================

The source tree deliberately uses a regular module layout. Keep new and modified code consistent with these conventions:

  - Keep each C module in matching `src/<module>.c` and `src/<module>.h` files, including small private headers used only for declarations.
  - Keep module-specific include guards in both files: `HAVE_PARALLEL_<MODULE>` for C files and `HAVE_PARALLEL_<MODULE>_H` for headers.
  - Prefix internal symbols with `php_parallel_` and lifecycle identifiers with `PARALLEL_`.
  - Define lifecycle hooks in pairs: `MINIT` with `MSHUTDOWN`, and `RINIT` with `RSHUTDOWN`. Keep the matching hook even when it is empty, and declare the pair in the module header.
  - Keep initialization hierarchical: `parallel.c` initializes top-level modules, and each module initializes its own submodules. Preserve lifecycle ordering unless a dependency requires changing it.
  - Use the `php_parallel_copy_string*` helpers for strings entering persistent or shared state. Allocate reusable internal keys and labels once as persistent interned strings during `MINIT`, rather than allocating them at each use.
  - Preserve the standard license banner and format C sources and headers with the repository's `.clang-format` configuration.

PHP Services
============

php-[dbg|gcov|asan|release]-[version]

PHP Versions
============

  - 8.0 (no asan support)
  - 8.1
  - 8.2
  - 8.3
  - 8.4

Parallel Services
=================

parallel-[dbg|gcov|asan|release]-[version]

Testing
=======

The various kinds of testing you may want to perform are all covered in docker-compose.yml.

In summary, `docker/parallel.test` is a helper for `run-tests.php`, arguments passed to it are forwarded to `run-tests.php`.

Executing:

    docker/parallel.test

Will execute the whole test suite.

Executing:

    docker/parallel.test -d opcache.enable_cli=1 -d opcache.enable=1 -d opcache.jit=disable

Will execute the test suite with opcache enabled (if enabled in the build), and without the JIT

Executing:

    docker/parallel.test --asan

Will execute the test suite with AddressSanitizer support (if enabled in the build).

Testing (an example)
====================

```sh
docker compose build php-asan-8.4
docker compose build parallel-asan-8.4
docker compose run --rm parallel-asan-8.4
phpize
./configure --enable-parallel
make
./docker/parallel.test --asan
```
