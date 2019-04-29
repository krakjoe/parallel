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
    * @throws \parallel\Runtime\Error                       if the thread could not be created
    * @throws \parallel\Runtime\Error\Bootstrap             if bootstrapping failed
    * Note: any Error leaves the Runtime unusable
    **/
    public function __construct(string $bootstrap = null);

    /**
    * Shall schedule a task for execution passing optional arguments
    * @throws \parallel\Runtime\Error                       if Runtime is unusable
    * @throws \parallel\Runtime\Error\Closed                if Runtime was closed
    * @throws \parallel\Runtime\Error\IllegalFunction       if task was created from internal function
    * @throws \parallel\Runtime\Error\IllegalInstruction    if task contains illegal instructions:
    *                                                            declare (anonymous) function
    *                                                            declare (anonymous) class
    *                                                            lexical by-reference access
    *                                                            yield
    * @throws \parallel\Runtime\Error\IllegalParameter      if task accepts or argv contains illegal variables:
    *                                                            object
    *                                                            resource (streams are cast to int where possible)
    *                                                            references
    * @throws \parallel\Runtime\Error\IllegalReturn         if task returns illegaly:
    *                                                            object
    *                                                            resource (streams are cast to int where possible)
    *                                                            references
    *                                                            caller ignored return (or throw)
    * Note: A Future shall only be returned if task contains a return (or throw) statement
    */
    public function run(Closure $task, array $argv = []) : ?\parallel\Future;
    
    /**
    * Shall request the Runtime shutdown
    * @throws \parallel\Runtime\Error                       if Runtime is unusable
    * @throws \parallel\Runtime\Error\Closed                if Runtime was closed
    * Note: Tasks scheduled for execution will be executed
    */
    public function close() : void;

    /**
    * Shall kill the Runtime
    * @throws \parallel\Runtime\Error                       if Runtime is unusable
    * @throws \parallel\Runtime\Error\Closed                if Runtime was closed
    * Note: Tasks scheduled for execution will not be executed,
    *    currently running task will be interrupted.
    * Note: Cannot interrupt internal function calls in progress
    */
    public function kill() : void;
}

final class parallel\Future {
    /**
    * Shall wait until the value is resolved
    * @throws \parallel\Future\Error                        if waiting for a value failed
    * @throws \parallel\Future\Error\Killed                 if task was killed
    * @throws \parallel\Future\Error\Foreign                if task raised an uncuaght unrecognized exception
    * Note: rethrows uncaught exception raised in task
    */
    public function value() : mixed;

    /**
    * Shall indicate if the Future value is resolved
    * @returns bool
    */
    public function done() : bool;
}

final class parallel\Channel {
    
    /**
    * Shall make an unbuffered channel with the given name
    * @param string name
    * @throws \parallel\Channel\Error\Existence             if channel already exists
    */
    public static function make(string $name) : Channel;
    
    /**
    * Shall make a buffered channel with the given name and capacity
    * @param string name
    * @param int capacity may be Channel::Infinite, or a positive integer
    * @throws \parallel\Channel\Error\Existence             if channel already exists
    */
    public static function make(string $name, int $capacity) : Channel;
    
    /**
    * Shall open the channel with the given name
    * @param string name a previously made channel
    * @throws \parallel\Channel\Error\Existence             if channel cannot be found
    */
    public static function open(string $name) : Channel;
    
    /**
    * Shall send the given value on this channel
    * @param mixed value any non-object, non-resource, non-null value
    * @throws \parallel\Channel\Error\Closed                if this channel is closed
    * @throws \parallel\Channel\Error\IllegalValue          if value is illegal (object, null)
    */
    public function send(mixed $value) : void;
    
    /**
    * Shall recv a value from this channel
    * @returns mixed
    * @throws \parallel\Channel\Error\Closed                if this channel is closed
    */
    public function recv() : mixed;
    
    /**
    * Shall close the channel
    * @throws \parallel\Channel\Error\Closed                if this channel was already closed
    */
    public function close() : void;
    
    public const Infinite;
}

final class parallel\Events implements \Countable, \Traversable {

    /**
    * Shall set Input for this event loop
    */
    public function setInput(Input $input) : void;

    /**
    * Shall watch for events on the given Channel
    * @throws \parallel\Events\Error\Existence              if the Channel was already added
    */
    public function addChannel(Channel $channel) : void;
    
    /**
    * Shall watch for events on the given Future
    * @throws \parallel\Events\Error\Existence              if the Future was already added
    */
    public function addFuture(string $name, Future $future) : void;
    
    /**
    * Shall remove the given target by name
    * @throws \parallel\Events\Error\Existence              if the target was not found
    */
    public function remove(string $target) : void;
    
    /**
    * Shall set the timeout
    * @param non-negative timeout in microseconds
    */
    public function setTimeout(int $timeout) : void;
    
    /**
    * Shall poll for the next event
    * @throws \parallel\Events\Error\Timeout                if timeout is used and reached
    */
    public function poll() : Event|false;
}

final class parallel\Events\Input {
    /**
    * Shall set input for the given target
    * @throws \parallel\Events\Input\Error\Existence        if input for target already exists
    * @throws \parallel\Events\Input\Error\IllegalValue     if value is illegal (object, null)
    */
    public function add(string $target, $value) : void;
    
    /**
    * Shall remove input for the given target
    * @throws \parallel\Events\Input\Error\Existence        if input for target does not exist
    */
    public function remove(string $target) : void;
    
    /**
    * Shall remove inputs for all targets
    */
    public function clear() : void;
}

final class parallel\Events\Event {
    /**
    * Event::Read or Event::Write
    */
    public int        $type;
    /**
    * Shall be the name of $object
    */
    public string    $source;
    /**
    * Shall be either Future or Channel
    */
    public object    $object;
    /**
    * Shall be set for Read
    */
    public            $value;
    
    const Read;
    const Write;
}
```

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

