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
	$parallel->run(function(DateTime $arg) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, DateTime $arg2) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, DateTime ... $arg3) {});
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function() : DateTime {});
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
--EXPECTF--
string(%d) "illegal parameter (DateTime) accepted by task at argument 1"
string(%d) "illegal parameter (DateTime) accepted by task at argument 2"
string(%d) "illegal parameter (DateTime) accepted by task at argument 3"
string(%d) "illegal return (DateTime) from task"
string(60) "illegal parameter (reference) accepted by task at argument 1"
string(60) "illegal parameter (reference) accepted by task at argument 2"
string(60) "illegal parameter (reference) accepted by task at argument 3"
string(36) "illegal return (reference) from task"


