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
		new DateTime
	]);
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2) {}, [
		1, new DateTime
	]);
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($arg, $arg2, ... $arg3) {}, [
		1, 2, new DateTime
	]);
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function($array) {}, [
		[new DateTime]
	]);
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal parameter (object) passed to task at argument 1"
string(%d) "illegal parameter (object) passed to task at argument 2"
string(%d) "illegal parameter (object) passed to task at argument 3"
string(%d) "illegal parameter (object) passed to task at argument 1"



