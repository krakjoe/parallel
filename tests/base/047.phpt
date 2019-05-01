--TEST--
parallel cancellation (ready)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$future = $parallel->run(function(){});

$future->value();

var_dump($future->cancel(), $future->cancelled());
?>
--EXPECT--
bool(false)
bool(false)


