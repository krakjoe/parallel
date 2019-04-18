--TEST--
Check group add non-channel
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
    $group->add(new stdClass);
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(26) "expected \parallel\Channel"


