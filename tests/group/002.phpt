--TEST--
Check group add non-future
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
    $group->add("future", new stdClass);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(36) "expected string and \parallel\Future"



