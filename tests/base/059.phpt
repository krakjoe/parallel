--TEST--
parallel class check cached
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
if (!version_compare(PHP_VERSION, "7.4", ">=")) {
    die("skip php 7.4 required");
}
?>
--FILE--
<?php
include sprintf("%s/059.inc", __DIR__);

$parallel = new \parallel\Runtime(sprintf("%s/059.inc", __DIR__));

$foo = new Foo();
$foo->std = new stdClass;
$foo->int = 42;

$parallel->run(function(Foo $d){
    echo "OK\n";
}, [$foo]);

$parallel->run(function(Foo $d){
    echo "OK\n";
}, [$foo]);
?>
--EXPECT--
OK
OK

