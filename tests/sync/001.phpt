--TEST--
Check sync __construct
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
var_dump(new \parallel\Sync);
var_dump(new \parallel\Sync(42));

try {
  new \parallel\Sync([]);
} catch (\parallel\Sync\Error\IllegalValue $ex) {
  var_dump($ex->getMessage());
}
?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
}
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  int(42)
}
string(36) "sync cannot contain non-scalar array"




