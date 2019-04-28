--TEST--
Check Events poll no arguments
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events;
use \parallel\Channel;

$events = new Events();
$events->addChannel(
    Channel::make("one", Channel::Infinite));

try {
    $events->poll(1);
} catch (\parallel\Error\InvalidArguments $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(21) "no arguments expected"


