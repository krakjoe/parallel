--TEST--
Check channel operation send on closed
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
   $channel->send(42);
} catch (\parallel\Channel\Error\Closed $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(18) "channel(io) closed"



