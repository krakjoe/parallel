--TEST--
Check Future value rejects objects with incompatible property layouts
--DESCRIPTION--
The worker runtime declares Message with three properties, the parent with
one. The task returns an instance, and the copy to the parent at value()
time would write past the end of the object. value() must reject it.
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
}

$runtime = new Runtime(__DIR__ . '/076.inc');

$future = $runtime->run(static function () {
    return new Message('a', 'b', 'c');
});

try {
    $future->value();
} catch (\parallel\Future\Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
parallel\Future\Error: cannot retrieve object of class Message, sending and receiving runtimes declare it differently
