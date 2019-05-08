--TEST--
Check basic channel operation duplicate name
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel  = Channel::make("io");

try {
    Channel::make("io");
} catch (\parallel\Channel\Error\Existence $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(31) "channel named io already exists"


