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
$runtimes = [
    new \parallel\Runtime,
    new \parallel\Runtime
];

$f[] = $runtimes[0]->run(function() use($sync){
  $sync(function() use($sync) {
    while ($sync->get() != 42) {
      $sync->wait();
    }
  });
  var_dump($sync);
});

$f[] = $runtimes[1]->run(function() use($sync) {
  $sync(function() use($sync) {
    $sync->set(42);
    $sync->notify();
  });
});

foreach ($f as $r)
    $r->value();

foreach ($runtimes as $r)
    $r->close();
?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}