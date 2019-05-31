--TEST--
Check anonymous Channel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

var_dump(new Channel);
var_dump(new Channel(Channel::Infinite));
var_dump(new Channel(1));
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php@%s"
  ["type"]=>
  string(10) "unbuffered"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php@%s"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  string(8) "infinite"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php@%s"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  int(1)
}
