--TEST--
Check basic channel operation (open)
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
	
	var_dump($channel);
}, [(string) $channel]);
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
}


