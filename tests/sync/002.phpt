--TEST--
Check sync get/set
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

  $sync(function() use($sync){
    var_dump($sync);
  });
});

\parallel\run(function() use($sync) {
  $sync(function() use($sync) {
    $sync->set(42);
    $sync->notify(true);
  });
});

$f = \parallel\run(function() use($sync) {
  $sync(function() use($sync) {
    while ($sync->get() != 42) {
      $sync->wait();
    }
  });

  $sync(function() use($sync){
    var_dump($sync);
  });
});

$f->value();
?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}