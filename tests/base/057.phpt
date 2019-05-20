--TEST--
parallel object check finds illegal property inline
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
if (ini_get("opcache.enable_cli")) {
    die("skip opcache must not be loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime;

class Foo {
    public $property;
}

$foo = new Foo();
$foo->property = new DateTime;

try {
    $parallel->run(function($foo){
    }, [$foo]);
} catch (parallel\Runtime\Error\IllegalParameter $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(52) "illegal parameter (Foo) passed to task at argument 1"


