--TEST--
Check Events poll no arguments
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events;
use \parallel\Channel;

$channel = 
    Channel::make("one", Channel::Infinite);
$channel->send([42]);

$events = new Events();
$events->addChannel($channel);

var_dump($events->poll());
?>
--EXPECTF--
object(parallel\Events\Event)#%d (%d) {
  ["type"]=>
  int(1)
  ["source"]=>
  string(3) "one"
  ["object"]=>
  object(parallel\Channel)#%d (%d) {
  }
  ["value"]=>
  array(1) {
    [0]=>
    int(42)
  }
}



