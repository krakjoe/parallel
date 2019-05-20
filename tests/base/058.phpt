--TEST--
parallel object check finds non-existent class
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

try {
    $parallel->run(function(DoesNotExist $d){

    });
} catch (\parallel\Runtime\Error\IllegalParameter $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(63) "illegal parameter (DoesNotExist) accepted by task at argument 1"

