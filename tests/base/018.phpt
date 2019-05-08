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
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, stdClass $arg2) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, stdClass ... $arg3) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function() : stdClass {});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(&$arg) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, &$arg2) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, & ... $arg3) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function & () : int {});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(57) "illegal parameter (object) accepted by task at argument 1"
string(57) "illegal parameter (object) accepted by task at argument 2"
string(57) "illegal parameter (object) accepted by task at argument 3"
string(33) "illegal return (object) from task"
string(60) "illegal parameter (reference) accepted by task at argument 1"
string(60) "illegal parameter (reference) accepted by task at argument 2"
string(60) "illegal parameter (reference) accepted by task at argument 3"
string(36) "illegal return (reference) from task"


