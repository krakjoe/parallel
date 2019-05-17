--TEST--
parallel check cache hit literal string
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
$future = $parallel->run(function(){
    return "Foo";
});

var_dump($future->value());
?>
--EXPECT--
string(3) "Foo"


