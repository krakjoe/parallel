--TEST--
Check events remove non-existent
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
    $events->remove("nothing");
} catch (\parallel\Events\Error\Existence $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(32) "target named "nothing" not found"


