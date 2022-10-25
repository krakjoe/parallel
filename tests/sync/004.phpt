--TEST--
Check sync destructor of refcounted value
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$sync = new \parallel\Sync;

$sync->set(sprintf("%s", "\\parallel\\first"));

$sync->set(sprintf("%s", "\\parallel\\second"));

var_dump($sync);
?>
--EXPECTF--
object(parallel\Sync)#%d (%d) {
  ["value"]=>
  string(16) "\parallel\second"
}