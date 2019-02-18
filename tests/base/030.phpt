--TEST--
parallel future exception
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();
$future   = $parallel->run(function(){
	throw new Exception();

	return false;
});

try {
	$future->value();
} catch (Exception $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
Fatal error: Uncaught Exception in %s030.php:4
Stack trace:
#0 [internal function]: \parallel\Runtime::run()
#1 {main}
  thrown in %s030.php on line 4
string(54) "an exception occured in Runtime, cannot retrieve value"



