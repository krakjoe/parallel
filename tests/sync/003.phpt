--TEST--
Check sync notify one
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$sync = new \parallel\Sync;

\parallel\run(function() use($sync){
  $sync(function() use($sync) {
    while ($sync->get() != 42) {
      $sync->wait();
    }
  });
  var_dump($sync);
});

$f = \parallel\run(function() use($sync) {
  $sync(function() use($sync) {
    $sync->set(42);
    $sync->notify();
  });
});

$f->value();

?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}