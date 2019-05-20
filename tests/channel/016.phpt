--TEST--
Check Channel share
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Runtime, Channel};

$runtime = new Runtime;

$future = $runtime->run(function($channel){
    return $channel;
}, [Channel::make("channel")]);

var_dump($future->value());
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(7) "channel"
  ["type"]=>
  string(10) "unbuffered"
}




