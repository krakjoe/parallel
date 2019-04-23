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
} catch (\parallel\Runtime\Error\IllegalFunction $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal function type (internal)"



