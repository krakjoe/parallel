--TEST--
Check channel operation close on closed
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel = Channel::make("io");
$channel->close();

try {
   $channel->close();
} catch (\parallel\Channel\Error\Closed $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(26) "channel(io) already closed"



