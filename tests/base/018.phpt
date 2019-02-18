--TEST--
Copy arginfo (FAIL)
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
	$parallel->run(function(stdClass $arg) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, stdClass $arg2) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, stdClass ... $arg3) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function() : stdClass {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(&$arg) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, &$arg2) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, & ... $arg3) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function & () : int {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(56) "illegal type (object) accepted by parallel at argument 1"
string(56) "illegal type (object) accepted by parallel at argument 2"
string(56) "illegal type (object) accepted by parallel at argument 3"
string(42) "illegal type (object) returned by parallel"
string(63) "illegal variable (reference) accepted by parallel at argument 1"
string(63) "illegal variable (reference) accepted by parallel at argument 2"
string(63) "illegal variable (reference) accepted by parallel at argument 3"
string(49) "illegal variable (reference) returned by parallel"
