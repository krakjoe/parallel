--TEST--
parallel Closure::fromCallable internal
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

try {
    $parallel->run(
        Closure::fromCallable('usleep'), [0.1]);
} catch (Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(51) "illegal function type (internal) passed to parallel"



