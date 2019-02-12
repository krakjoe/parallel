parallel
========

[![Build Status](https://travis-ci.org/krakjoe/parallel.svg?branch=master)](https://travis-ci.org/krakjoe/parallel)
[![Coverage Status](https://coveralls.io/repos/github/krakjoe/parallel/badge.svg?branch=master)](https://coveralls.io/github/krakjoe/parallel)

A succint parallel concurrency API for PHP 7:

```php
class parallel\Runtime {
	/*
	* Shall construct a new Runtime
	* @param string bootstrap (generally an autoloader)
	* @param array  ini configuration
	*/	
	public function __construct(string $bootstrap, array $configuration);
	/**
	* Shall construct a new Runtime
	* @param string bootstrap (generally an autoloader)
	**/
	public function __construct(string $bootstrap);
	/**
	* Shall construct a new Runtime
	* @param array ini configuration
	**/
	public function __construct(array $configuration);

	/*
	* Shall schedule a Closure for executing, optionally passing parameters and
	* returning a Future value
	* @param Closure handler
	* @param argv
	*/
	public function run(Closure $handler, array $args = []) : \parallel\Future;
	
	/*
	* Shall request the Runtime shutdown
	* Note: anything scheduled for execution will be executed
	*/
	public function close() : void;
}

class parallel\Future {
	/*
	* Shall wait until the value becomes available
	*/
	public function value() : mixed;
}
```

Implementation
==============

Tomorrow ... sleep now ...


