--TEST--
Check group remove non-existent
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$group = new \parallel\Group();

try {
    $group->remove("nothing");
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(32) "object named "nothing" not found"


