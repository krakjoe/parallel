--TEST--
Check serialize Channel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

try {
    serialize(new Channel);
} catch (Exception $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(50) "Serialization of 'parallel\Channel' is not allowed"


