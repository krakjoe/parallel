--TEST--
parallel class check invalid member
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
include sprintf("%s/060.inc", __DIR__);

$parallel = new \parallel\Runtime(sprintf("%s/060.inc", __DIR__));

$foo = new Foo();
$foo->date = new DateTime;

try {
    $parallel->run(function(Foo $foo){
    }, [$foo]);
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(52) "illegal parameter (Foo) passed to task at argument 1"


