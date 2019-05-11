--TEST--
Check Events Future killed
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

$runtime = new Runtime;
$future  = $runtime->run(function(){
    while (1)
        usleep(1000);
    
    return 42;
});

$runtime->kill();

$events = new Events();
$events->addFuture("future", $future);

var_dump($events->poll());
?>
--EXPECTF--
object(parallel\Events\Event)#%d (%d) {
  ["type"]=>
  int(6)
  ["source"]=>
  string(6) "future"
  ["object"]=>
  object(parallel\Future)#%d (%d) {
    ["runtime"]=>
    object(parallel\Runtime)#%d (%d) {
    }
  }
  ["value"]=>
  NULL
}

