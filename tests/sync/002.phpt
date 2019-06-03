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

  var_dump($sync);
});

$f->value();

$sync->set("parallel\\sync");
$sync->set("parallel\\sync"); # not a mistake, testing dtor

var_dump($sync->get());

# test notify one
$f = \parallel\run(function() use($sync) {
  $sync(function() use($sync) {
    while ($sync->get() == "parallel\\sync") {
      $sync->wait();
    }  
  });
});

$sync(function() use($sync){
  $sync->set(42);
  $sync->notify();
});

$f->value();

$sync->set("parallel\\sync"); # test release string

try {
  $sync->set([]);
} catch (\parallel\Sync\Error\IllegalValue $ex) {
  var_dump($ex->getMessage());
}
?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}
string(%d) "parallel\sync"
string(%d) "sync cannot contain non-scalar array"
