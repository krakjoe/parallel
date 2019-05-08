--TEST--
Check channel make arguments
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
    Channel::make("name", -2);
} catch (TypeError $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(55) "capacity may be -1 for unlimited, or a positive integer"




