Contributing
============

To ease reproduction of test cases, development, and CI, parallel has adopted docker (with composition) as a core part of the development workflow.

parallel requires a ZTS build of PHP which is built as a separate environment upon which the testing environment relies.

PHP Services
============

php-[dbg|gcov|asan|release]-[version]

PHP Versions
============

  - 8.0 (no asan support)
  - 8.1
  - 8.2

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

See workflow files for extensive exemplary builds.