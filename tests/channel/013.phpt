--TEST--
Check Events count
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

if (count($events) == 1) {
    echo "OK";
}
?>
--EXPECT--
OK

