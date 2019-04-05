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
} catch (Throwable $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(51) "channel named io already exists, did you mean open?"


