parallel
========

[![Build Status](https://travis-ci.org/krakjoe/parallel.svg?branch=develop)](https://travis-ci.org/krakjoe/parallel)
[![Build status](https://ci.appveyor.com/api/projects/status/cppfcu6unc0r0h0b?svg=true)](https://ci.appveyor.com/project/krakjoe/parallel)
[![Coverage Status](https://coveralls.io/repos/github/krakjoe/parallel/badge.svg?branch=develop)](https://coveralls.io/github/krakjoe/parallel)

A succinct parallel concurrency API for PHP 7:

```php
final class parallel\Runtime {
	/**
	* Shall construct a new Runtime
	* @param string bootstrap (generally an autoloader)
	* @throws \parallel\Exception if arguments are invalid
	* @throws \parallel\Exception if bootstrapping failed
	* @throws \parallel\Exception if the thread could not be created	
	**/
	public function __construct(string $bootstrap = null);

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
	* @throws \parallel\Future\Timeout if timeout is reached
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

final class parallel\Events implements \Traversable {

    /*
    * Shall set Input for this event loop
    */
    public function setInput(Input $input) : void;

    /*
    * Shall watch for events on the given Channel
    * @throws \parallel\Exception if the Channel was already added
    */
    public function addChannel(Channel $channel) : void;
    
    /*
    * Shall watch for events on the given Future
    * @throws \parallel\Exception if the Future was already added
    */
    public function addFuture(string $name, Future $future) : void;
    
    /*
    * Shall remove the given target by name
    * @throws \parallel\Exception if the object was not found
    */
    public function remove(string $target) : void;
    
    /*
    * Shall set the timeout
    * @param non-negative timeout in microseconds
    * Note: timeouts are not enabled by default
    */
    public function setTimeout(int $timeout) : void;
    
    /*
    * Shall poll for the next event
    */
    public function poll() : Event|false;
}

final class parallel\Events\Input {
    /*
    * Shall set input for the given target
    * Note: target should be the name of a Channel or Future
    */
    public function add(string $target, $value) : void;
    
    /*
    * Shall remove input for the given target
    * Note: target should be the name of a Channel or Future
    */
    public function remove(string $target) : void;
    
    /*
    * Shall remove inputs for all targets
    */
    public function clear() : void;
}

final class parallel\Events\Event {
    /*
    * Event::Read or Event::Write
    */
    public int     $type;
    /*
    * Shall be the name of $object
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

