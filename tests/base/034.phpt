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
	$parallel = new \parallel\Runtime(sprintf("%s/bootstrapc.php", __DIR__));
} catch (Exception $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "bootstrapping Runtime failed with %sbootstrapc.php"




