--TEST--
parallel recursion in arrays in the closure
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
  die("skip parallel not loaded");
}
if (!version_compare(PHP_VERSION, "8.1", ">=")) {
    die("skip php 8.1 required");
}
?>
--FILE--
<?php

$app = function ($n) {
  return transformChunk($n);
};

$runtime = new \parallel\Runtime(__DIR__.'/072-bootstrap.php');
$future = $runtime->run(
  $app,
  [
    10
  ]
);
var_dump($future->value());

$runtime = new \parallel\Runtime(__DIR__.'/072-bootstrap.php');
$future = $runtime->run(
  $app,
  [
    10
  ]
);
var_dump($future->value());

?>
--EXPECT--
int(55)
int(55)
--XFAIL--
REASON: no cyclic reference collector implemented yet
