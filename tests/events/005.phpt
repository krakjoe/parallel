--TEST--
Check events remove
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$events = new \parallel\Events();
$channel = 
    \parallel\Channel::make("buffer");
$events->addTargetChannel($channel);

try {
    $events->removeTarget("buffer");
    $events->removeTarget("buffer");
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage(), $ex->getLine());
}
?>
--EXPECT--
string(31) "target named "buffer" not found"
int(9)



