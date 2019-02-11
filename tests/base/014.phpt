--TEST--
ZEND_DECLARE_FUNCTION
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
		function test() {}
	});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(55) "illegal instruction (function) on line 1 of entry point"


