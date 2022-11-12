parallel
========

[![Linux](https://github.com/krakjoe/parallel/actions/workflows/linux.yml/badge.svg)](https://github.com/krakjoe/parallel/actions/workflows/linux.yml)
[![AddressSanitizer](https://github.com/krakjoe/parallel/actions/workflows/asan.yml/badge.svg)](https://github.com/krakjoe/parallel/actions/workflows/asan.yml)
[![Windows](https://github.com/krakjoe/parallel/actions/workflows/windows.yml/badge.svg)](https://github.com/krakjoe/parallel/actions/workflows/windows.yml)
[![Coverage Status](https://coveralls.io/repos/github/krakjoe/parallel/badge.svg?branch=develop)](https://coveralls.io/github/krakjoe/parallel)

A succinct parallel concurrency API for PHP 8

Documentation
=============

Documentation can be found in the PHP manual: https://php.net/parallel

Requirements and Installation
=============================

See [INSTALL.md](INSTALL.md)

Hello World
===========

```php
<?php
$runtime = new \parallel\Runtime();

$future = $runtime->run(function(){
    for ($i = 0; $i < 500; $i++)
        echo "*";

    return "easy";
});

for ($i = 0; $i < 500; $i++) {
    echo ".";
}

printf("\nUsing \\parallel\\Runtime is %s\n", $future->value());
```

This may output something like (output abbreviated):

```
.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*
Using \parallel\Runtime is easy
```

Development
===========

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on contribution and development (and debugging).
