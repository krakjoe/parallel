--TEST--
ZEND_DECLARE_LAMBDA_FUNCTION
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new parallel\Runtime();

try {
	$runtime->run(function() {
		function() {};
	});
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal instruction (function) on line 1 of task"


