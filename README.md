parallel
========

[![Build Status](https://travis-ci.org/krakjoe/parallel.svg?branch=develop)](https://travis-ci.org/krakjoe/parallel)
[![Build status](https://ci.appveyor.com/api/projects/status/cppfcu6unc0r0h0b?svg=true)](https://ci.appveyor.com/project/krakjoe/parallel)
[![Coverage Status](https://coveralls.io/repos/github/krakjoe/parallel/badge.svg?branch=develop)](https://coveralls.io/github/krakjoe/parallel)

A succinct parallel concurrency API for PHP 7

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

