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
} catch (\parallel\Future\Error\Uncaught $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
Fatal error: Uncaught Exception in %s:4
Stack trace:
#0 {main}
  thrown in %s on line 4
string(21) "cannot retrieve value"



