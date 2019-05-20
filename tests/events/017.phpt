--TEST--
Check Events Channel closed
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--INI--
opcache.optimization_level=0
--FILE--
<?php
use \parallel\Events;
use \parallel\Runtime;
use \parallel\Channel;

$runtime = new Runtime;
$sync    = Channel::make("sync");

$runtime->run(function(){
    $sync = Channel::open("sync");
    $sync->close();
});

$events = new Events();
$events->addChannel($sync);

var_dump($events->poll());
?>
--EXPECTF--
object(parallel\Events\Event)#%d (%d) {
  ["type"]=>
  int(3)
  ["source"]=>
  string(4) "sync"
  ["object"]=>
  object(parallel\Channel)#%d (%d) {
    ["name"]=>
    string(4) "sync"
    ["type"]=>
    string(10) "unbuffered"
  }
  ["value"]=>
  NULL
}


