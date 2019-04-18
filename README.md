parallel
========

[![Build Status](https://travis-ci.org/krakjoe/parallel.svg?branch=develop)](https://travis-ci.org/krakjoe/parallel)
[![Build status](https://ci.appveyor.com/api/projects/status/cppfcu6unc0r0h0b?svg=true)](https://ci.appveyor.com/project/krakjoe/parallel)
[![Coverage Status](https://coveralls.io/repos/github/krakjoe/parallel/badge.svg?branch=develop)](https://coveralls.io/github/krakjoe/parallel)

A succinct parallel concurrency API for PHP 7:

```php
final class parallel\Runtime {
	/*
	* Shall construct a new Runtime
	* @param string bootstrap (generally an autoloader)
	* @param array  ini configuration
	* @throws \parallel\Exception if arguments are invalid
	* @throws \parallel\Exception if bootstrapping failed
	* @throws \parallel\Exception the thread could not be created
	*/
	public function __construct(string $bootstrap, array $configuration);
	/**
	* Shall construct a new Runtime
	* @param string bootstrap (generally an autoloader)
	* @throws \parallel\Exception if arguments are invalid
	* @throws \parallel\Exception if bootstrapping failed
	* @throws \parallel\Exception if the thread could not be created	
	**/
	public function __construct(string $bootstrap);
	/**
	* Shall construct a new Runtime
	* @throws \parallel\Exception if arguments are invalid
	* @throws \parallel\Exception if the thread could not be created
	* @param array ini configuration
	**/
	public function __construct(array $configuration);

	/*
	* Shall schedule a task for execution passing optional arguments
	* @param Closure task
	* @param array argv
	* @throws \parallel\Exception if \parallel\Runtime was closed
	* @throws \parallel\Exception if \parallel\Runtime is in unusable state
	* @throws \parallel\Exception if task contains illegal instructions
	* @throws \parallel\Exception if return from task is ignored
	* Note: A Future shall only be returned if task contains a return statement
	*	Should the caller ignore the return statement in task, an exception
	*	shall be thrown
	*/
	public function run(Closure $task, array $argv = []) : ?\parallel\Future;
	
	/*
	* Shall request the Runtime shutdown
	* Note: Tasks scheduled for execution will be executed
	* @throws \parallel\Exception if \parallel\Runtime was already closed
	*/
	public function close() : void;

	/*
	* Shall kill the Runtime
	* Note: Tasks scheduled for execution will not be executed,
	*	currently running task will be interrupted.
	* Note: Cannot interrupt internal function calls in progress
	* @throws \parallel\Exception if \parallel\Runtime was already closed
	*/
	public function kill() : void;
}

final class parallel\Future {
	/*
	* Shall wait until the value is resolved
	* @throws \parallel\Exception if task was killed
	* @throws \parallel\Exception if \parallel\Runtime was closed before execution
	* @throws \parallel\Exception if \parallel\Runtime was killed during execution
	* @throws \parallel\Exception if task suffered a fatal error or exception
	*/
	public function value() : mixed;

	/*
	* Shall wait until the value is resolved or the timeout is reached
	* @param non-negative timeout in microseconds
	* @throws \parallel\Exception if task was killed
	* @throws \parallel\Exception if \parallel\Runtime was closed before execution
	* @throws \parallel\Exception if \parallel\Runtime was killed during execution
	* @throws \parallel\Exception if task suffered a fatal error or exception
	* @throws \parallel\Exception if timeout is negative
	* @throws \parallel\TimeoutException if timeout is reached
	*/
	public function value(int $timeout) : mixed;

	/*
	* Shall indicate if the Future value is resolved
	* @returns bool
	*/
	public function done() : bool;
}

final class parallel\Channel {
    
    /*
    * Shall make an unbuffered channel with the given name
    * @param string name
    * @throws \parallel\Exception if channel already exists
    */
    public static function make(string $name) : Channel;
    
    /*
    * Shall make a buffered channel with the given name and capacity
    * @param string name
    * @param int    capacity may be Channel::Infinite, or a positive integer
    * @throws \parallel\Exception if arguments are invalid
    * @throws \parallel\Exception if channel already exists
    */
    public static function make(string $name, int $capacity) : Channel;
    
    /*
    * Shall open the channel with the given name
    * @param string name a previously made channel
    * @throws \parallel\Exception if the channel cannot be found
    */
    public static function open(string $name) : Channel;
    
    /*
    * Shall send the given value on this channel
    * @param mixed value any non-object, non-resource, non-null value
    * @throws \parallel\Channel\Closed if this channel is closed
    */
    public function send(mixed $value) : void;
    
    /*
    * Shall recv a value from this channel
    * @returns mixed
    * @throws \parallel\Channel\Closed if this channel is closed
    */
    public function recv() : mixed;
    
    /*
    * Shall close the channel
    * @throws \parallel\Channel\Closed if this channel was already closed
    */
    public function close() : void;
    
    public const Infinite;
}

final class parallel\Group {
    /*
    * Shall construct a new Group
    */
    public function __construct();
    
    /*
    * Shall construct a new group from the given array
    * Note: Elements of $group should be in the form:
    *       string $name => Future|Channel $object
    */
    public function __construct(array $group);
    
    /*
    * Shall add the given Channel to this Group
    * @throws \parallel\Exception if the Channel was already added
    */
    public function add(Channel $channel) : void;
    
    /*
    * Shall add the given Future to this Group
    * @throws \parallel\Exception is the Future was already added
    */
    public function add(string $name, Future $future) : void;
    
    /*
    * Shall remove the object with the given name from this Group
    * @throws \parallel\Exception if the object was not found
    */
    public function remove(string $name);

    /*
    * Shall perform non-blocking reads on this Group
    * Note: returns Result for the first succesful operation, returns
    *       false when there are no operations left to perform 
    */
    public function perform() : Result|false;
    
    /*
    * Shall perform non-blocking reads and writes on this Group
    * Note: Elements of $payloads should be in the form:
    *       string $name => $value
    *       Where an object is included in this Group and named
    *       in payloads, a non-blocking write is performed.
    *       Where an object is not included payloads, a non-blocking
    *       read is performed.
    *       Where a write succeeds, it is removed from payloads and this Group.
    *       Where a read succeeds, it is removed from this Group.
    */
    public function perform(array &$payloads) : Result|false;
}

final class parallel\Group\Result {
    /*
    * Shall be either Result::Read or Result::Write
    */
    public int     $type;
    /*
    * Shall be the name $object had in Group
    */
    public string  $source;
    /*
    * Shall be either Future or Channel
    */
    public object  $object;
    /*
    * Shall be set for Read
    */
    public         $value;
    
    const Read;
    const Write;
}
```

Implementation
==============

In PHP there was only one kind of parallel concurrency extension API, the kind that pthreads and pht try to implement:

  * They are hugely complicated
  * They are error prone
  * They are difficult to understand
  * They are difficult to deploy
  * They are difficult to maintain

`parallel` takes a totally different approach to these two by providing only a very small, easy to understand API that exposes the power of parallel concurrency without any of the aforementioned headaches.

By placing some restrictions upon what a task intended for parallel execution can do, we remove almost all of the cognitive overhead of using threads, we hide away completely any mutual exclusion and all the things a programmer using threads must think about.

In practice this means tasks intended for parallel execution must not:

  * accept or return by reference
  * accept or return objects
  * execute a limited set of instructions

Instructions prohibited directly in tasks intended for parallel execution are:

  * declare (anonymous) function
  * declare (anonymous) class
  * lexical scope access
  * yield

__No instructions are prohibited in the files which the task may include.__


Requirements and Installation
=============================

See [INSTALL.md](INSTALL.md)


Configuring the Runtime
=======================

The optional bootstrap file passed to the constructor will be included before any code is executed, generally this will be an autoloader.

The configuration array passed to the constructor will configure the Runtime using INI.

The following options may be an array, or comma separated list:

  * disable_functions
  * disable_classes

The following options will be ignored:

  * extension - use dl()
  * zend_extension - unsupported

All other options are passed directly to zend verbatim and set as if set by system configuration file.

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

