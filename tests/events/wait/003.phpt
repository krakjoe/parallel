--TEST--
Check events timeout
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;
use \parallel\Events;

$channel = Channel::make("channel");

$events = new Events();

$events->addChannel($channel);

$events->setTimeout(100000);

try {
    foreach ($events as $event) {
        
    }
} catch (\parallel\Events\Error\Timeout $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(15) "timeout occured"






