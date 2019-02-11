parallel
========

A succint parallel concurrency API for PHP 7:

```php
class parallel\Runtime {
	/*
	* Shall construct a new parallel runtime, optionally bootstrapping and configuring
	* @param string bootstrap file (generally an autoloader)
	* @param array  ini configuration for the Runtime
	*/	
	public function __construct(string $bootstrap = null, array $configuration = []);

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


