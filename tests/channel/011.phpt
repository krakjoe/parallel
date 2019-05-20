--TEST--
Check channel send arguments
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel = Channel::make("buffer", Channel::Infinite);

try {
    $channel->send(new DateTime);
} catch (\parallel\Channel\Error\IllegalValue $th) {
    var_dump($th->getMessage());
}
?>
--EXPECT--
string(33) "value of type DateTime is illegal"





