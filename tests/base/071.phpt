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
function transformChunk($n)
{
  $fibonacci = function ($n) use (&$fibonacci) {
    if ($n == 0) {
      return 0;
    }
    if ($n == 1) {
      return 1;
    }
    return $fibonacci($n - 1) + $fibonacci($n - 2);
  };
  return $fibonacci($n);
}

$runtime = new \parallel\Runtime();
$future = $runtime->run(
  transformChunk(...),
  [
    10
  ]
);
var_dump($future->value());

$runtime = new \parallel\Runtime();
$future = $runtime->run(
  transformChunk(...),
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
