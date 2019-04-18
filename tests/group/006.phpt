--TEST--
Check group add bad arguments
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
    $group->add(1,2,3);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(58) "expected string and \parallel\Future, or \parallel\Channel"




