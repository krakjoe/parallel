--TEST--
parallel check arginfo cached
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$closure = function(Closure $closure){

};

$parallel->run($closure, [function(){}]);
$parallel->run($closure, [function(){}]);
echo "OK\n";
?>
--EXPECT--
OK


