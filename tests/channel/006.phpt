--TEST--
Check basic channel operation non-existent name
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
    Channel::open("io");
} catch (Throwable $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(50) "channel named io is not available, was it closed ?"



