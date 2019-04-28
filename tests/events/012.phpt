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

var_dump($events->poll());
?>
--EXPECTF--
object(parallel\Events\Event)#5 (4) {
  ["type"]=>
  int(1)
  ["source"]=>
  string(6) "future"
  ["object"]=>
  object(parallel\Future)#3 (0) {
  }
  ["value"]=>
  object(RuntimeException)#4 (7) {
    ["message":protected]=>
    string(0) ""
    ["string":"Exception":private]=>
    string(0) ""
    ["code":protected]=>
    int(0)
    ["file":protected]=>
    string(%d) "%s"
    ["line":protected]=>
    int(7)
    ["trace":"Exception":private]=>
    array(0) {
    }
    ["previous":"Exception":private]=>
    NULL
  }
}
