--TEST--
Copy argv (FAIL)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

try {
	$parallel->run(function($arg) {}, [
		new stdClass
	]);
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2) {}, [
		1, new stdClass
	]);
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, ... $arg3) {}, [
		1, 2, new stdClass
	]);
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($array) {}, [
		[new stdClass]
	]);
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(58) "illegal variable (object) passed to parallel at argument 1"
string(58) "illegal variable (object) passed to parallel at argument 2"
string(58) "illegal variable (object) passed to parallel at argument 3"
string(58) "illegal variable (object) passed to parallel at argument 1"



