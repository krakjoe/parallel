Contributing
============

To ease reproduction of test cases, development, and CI, parallel has adopted docker (with composition) as a core part of the development workflow.

parallel requires a ZTS build of PHP which is built as a separate environment upon which the testing environment relies.

Executing:

    docker-compose build parallel-8.0


Will build a php-8.0 environment (if not available), then build a parallel-8.0 image, and so executing:

    docker-compose run parallel-8.0

Will enter into a testing environment with PHP-8.0 and parallel installed.

Build Arguments
===============

| Name              | Default           | Options          |
|-------------------|-------------------|------------------|
| PHP_SRC_TYPE      | dist              | dist, git        |
| PHP_SRC_DEBUG     | enable            | enable, disable  |
| PHP_SRC_OPCACHE   | disable           | enable, disable  |
| PHP_SRC_ASAN      | disable           | enable, disable  |
| PHP_VERSION_MAJOR | -                 | numeric          |
| PHP_VERSION_MINOR | -                 | numeric          |
| PHP_VERSION_MINOR | -                 | numeric          |
| PHP_VERSION_RC    | -                 | alphanumeric     |

Testing
=======

The various kinds of testing you may want to perform are all covered in workflow files.

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