--TEST--
Check basic channel operation (argument)
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

$parallel = new parallel\Runtime();
$parallel->run(function($channel){
	var_dump($channel);
	
}, [(string) $channel]);
?>
--EXPECT--
string(2) "io"

