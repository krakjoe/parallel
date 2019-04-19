--TEST--
Check events add bad arguments
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
    $events->addTargetChannel(1,2,3);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}

try {
    $events->addTargetFuture(1,2,3);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(26) "expected \parallel\Channel"
string(41) "expected target name and \parallel\Future"




