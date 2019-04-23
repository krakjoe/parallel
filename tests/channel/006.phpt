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
} catch (\parallel\Channel\Error\Existence $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(26) "channel named io not found"



