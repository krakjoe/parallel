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
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return null;
	});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return 42;
	});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}

try {
	$parallel->run(function(){
		return $var;
	});
} catch (Error $ex) {
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
--EXPECT--
string(89) "return on line 1 of entry point ignored by caller, caller must retain reference to Future"
string(89) "return on line 1 of entry point ignored by caller, caller must retain reference to Future"
string(89) "return on line 1 of entry point ignored by caller, caller must retain reference to Future"
string(89) "return on line 1 of entry point ignored by caller, caller must retain reference to Future"
OK




