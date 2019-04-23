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
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal instruction (function) on line 1 of task"


