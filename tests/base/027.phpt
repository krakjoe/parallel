--TEST--
Future may not be constructed
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
try {
    new \parallel\Future();
} catch (\parallel\Future\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(45) "construction of Future objects is not allowed"

