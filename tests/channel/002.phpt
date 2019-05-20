--TEST--
Check make/open/cast
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
	$channel = Channel::open($channel);
	
	var_dump((string)$channel);

}, [(string) $channel]);
?>
--EXPECT--
string(2) "io"


