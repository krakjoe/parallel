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
--EXPECT--
string(57) "illegal parameter (DateTime) passed to task at argument 1"
string(57) "illegal parameter (DateTime) passed to task at argument 2"
string(57) "illegal parameter (DateTime) passed to task at argument 3"
string(57) "illegal parameter (DateTime) passed to task at argument 1"


