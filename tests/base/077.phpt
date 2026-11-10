--TEST--
Check task arguments reject objects with incompatible property layouts
--DESCRIPTION--
The parent declares Message with three properties, the worker runtime with
one. The argument is copied into the worker when the task is scheduled,
which would write past the end of the object. The task must fail with the
rejection instead of corrupting memory, and value() must rethrow it.
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use parallel\Runtime;

class Message {
    public $a;
    public $b;
    public $c;

    public function __construct($a, $b, $c) {
        $this->a = $a;
        $this->b = $b;
        $this->c = $c;
    }
}

$runtime = new Runtime(__DIR__ . '/077.inc');

$future = $runtime->run(static function ($message) {
    return $message;
}, [new Message('a', 'b', 'c')]);

try {
    $future->value();
} catch (\parallel\Runtime\Error\IllegalParameter $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
parallel\Runtime\Error\IllegalParameter: cannot pass object of class Message to the task, sending and receiving runtimes declare it differently
