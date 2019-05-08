--TEST--
Check events add duplicate
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$events = new \parallel\Events();
$channel = \parallel\Channel::make("buffer");

try {
    $events->addChannel($channel);
    $events->addChannel($channel);
} catch (\parallel\Events\Error\Existence $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(35) "target named "buffer" already added"




