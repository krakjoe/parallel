--TEST--
parallel object check finds illegal property in table
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

$std = new stdClass;
$std->date = new DateTime;

try {
    $parallel->run(function($std){
    }, [$std]);
} catch (parallel\Runtime\Error\IllegalParameter $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(57) "illegal parameter (stdClass) passed to task at argument 1"


