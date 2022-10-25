--TEST--
Check sync illegal value
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$sync = new \parallel\Sync;

try {
  $sync->set([]);
} catch (\parallel\Sync\Error\IllegalValue $ex) {
  var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "sync cannot contain non-scalar array"