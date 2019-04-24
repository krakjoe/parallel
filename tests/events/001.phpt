--TEST--
Check events add non-channel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$events = new \parallel\Events();

try {
    $events->addChannel(new stdClass);
} catch (\parallel\Error\InvalidArguments $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(26) "expected \parallel\Channel"


