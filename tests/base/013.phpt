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
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(55) "illegal instruction (function) on line 1 of entry point"


