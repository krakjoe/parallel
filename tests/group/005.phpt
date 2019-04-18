--TEST--
Check group remove
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$group = new \parallel\Group();
$channel = 
    \parallel\Channel::make("buffer");
$group->add($channel);

try {
    $group->remove("buffer");
    $group->remove("buffer");
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage(), $ex->getLine());
}
?>
--EXPECT--
string(31) "object named "buffer" not found"
int(9)



