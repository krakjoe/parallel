--TEST--
ZEND_YIELD
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();
$var     = null;

try {
	$parallel->run(function() {
		yield;
	});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(52) "illegal instruction (yield) on line 1 of entry point"


