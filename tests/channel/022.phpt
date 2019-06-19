--TEST--
Check clone Channel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel = new Channel;
$clone   = clone $channel;

var_dump($clone, $channel);
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s[1]"
  ["type"]=>
  string(10) "unbuffered"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s[1]"
  ["type"]=>
  string(10) "unbuffered"
}

