--TEST--
Check Events non-blocking
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
$events->addChannel(Channel::make("buffer"));
$events->setBlocking(false);

if ($events->poll() === null && count($events)) {
    echo "OK";
}
?>
--EXPECT--
OK


