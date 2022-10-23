--TEST--
Check Events Future error
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events;
use \parallel\Runtime;

$runtime = new Runtime;
$future  = $runtime->run(function(){
    throw new RuntimeException();
});

$events = new Events();
$events->addFuture("future", $future);

$event = $events->poll();

if (!$event->value instanceof RuntimeException) {
  echo "FAIL \n";
  var_dump($event);
  exit;
}

echo "OK";
?>
--EXPECT--
OK