--TEST--
parallel bootstrap fail
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
try {
	$parallel = new \parallel\Runtime(sprintf("%s/nope.php", __DIR__));
} catch (\parallel\Runtime\Error\Bootstrap $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
Warning: Unknown: failed to open stream: No such file or directory in Unknown on line 0
string(%d) "bootstrapping failed with %snope.php"


