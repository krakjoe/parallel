--TEST--
Check channel operation recv on closed
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
   $channel->recv();
} catch (\parallel\Channel\Error\Closed $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(18) "channel(io) closed"



