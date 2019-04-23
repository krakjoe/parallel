--TEST--
parallel bootstrap compile error
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php

try {
	$parallel = new \parallel\Runtime(sprintf("%s/bootstrapc.inc", __DIR__));
} catch (\parallel\Runtime\Error\Bootstrap $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "bootstrapping failed with %sbootstrapc.inc"




