--TEST--
Check group add duplicate
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$group = new \parallel\Group();
$channel = \parallel\Channel::make("buffer");

try {
    $group->add($channel);
    $group->add($channel);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(35) "object named "buffer" already added"




