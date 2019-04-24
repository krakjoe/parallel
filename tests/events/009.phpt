--TEST--
Check Event constructor disallowed
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
try {
    new \parallel\Events\Event();
} catch (\parallel\Events\Event\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(51) "construction of Events\Event objects is not allowed"


