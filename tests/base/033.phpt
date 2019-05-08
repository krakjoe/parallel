--TEST--
parallel bootstrap exception
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php

try {
	$parallel = new \parallel\Runtime(sprintf("%s/bootstrape.inc", __DIR__));
} catch (\parallel\Runtime\Error\Bootstrap $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "bootstrapping failed with %sbootstrape.inc"




