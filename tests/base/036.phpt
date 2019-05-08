--TEST--
parallel return values
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

try {
	$parallel->run(function(){
		return;
	});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return null;
	});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return 42;
	});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return $var;
	});
} catch (\parallel\Runtime\Error\IllegalReturn $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		echo "OK\n";
	});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "return on line 1 of task ignored by caller, caller must retain reference to Future"
string(%d) "return on line 1 of task ignored by caller, caller must retain reference to Future"
string(%d) "return on line 1 of task ignored by caller, caller must retain reference to Future"
string(%d) "return on line 1 of task ignored by caller, caller must retain reference to Future"
OK




