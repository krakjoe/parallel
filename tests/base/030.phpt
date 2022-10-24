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

$future->value();
?>
--EXPECTF--
Fatal error: Uncaught Exception in %s:4
Stack trace:
%A