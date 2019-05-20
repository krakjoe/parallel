--TEST--
Check Channel debug
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

var_dump(Channel::make("unbuffered"));

var_dump(Channel::make("buffered", 1));

var_dump(Channel::make("infinite", Channel::Infinite));

$channel = Channel::make("contents", 1);
$channel->send(1);

var_dump($channel);
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(10) "unbuffered"
  ["type"]=>
  string(10) "unbuffered"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(8) "buffered"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  int(1)
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(8) "infinite"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  string(8) "infinite"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(8) "contents"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  int(1)
  ["size"]=>
  int(1)
}



