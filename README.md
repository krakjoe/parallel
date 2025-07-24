parallel
========

[![Linux](https://img.shields.io/github/actions/workflow/status/krakjoe/parallel/linux.yml?branch=develop&style=for-the-badge&logo=github%20actions&label=Linux)](https://github.com/krakjoe/parallel/actions/workflows/linux.yml)
[![AddressSanitizer](https://img.shields.io/github/actions/workflow/status/krakjoe/parallel/asan.yml?branch=develop&style=for-the-badge&logo=github%20actions&label=ASAN)](https://github.com/krakjoe/parallel/actions/workflows/asan.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/krakjoe/parallel/windows.yml?branch=develop&style=for-the-badge&logo=github%20actions&label=Windows)](https://github.com/krakjoe/parallel/actions/workflows/windows.yml)
[![Coverage Status](https://img.shields.io/coverallsCoverage/github/krakjoe/parallel?branch=develop&style=for-the-badge&logo=coveralls)](https://coveralls.io/github/krakjoe/parallel)

A succinct parallel concurrency API for PHP 8

[![GoFundMe](https://img.shields.io/badge/GoFundMe-00B964.svg?style=for-the-badge&logo=GoFundMe&logoColor=white)](https://gofund.me/c34f3dde)

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
