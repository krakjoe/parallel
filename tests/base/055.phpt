--TEST--
parallel task check cached, Future used second time
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

$closure = function() {
    echo "OK\n";
};

$parallel->run($closure);

$future = $parallel->run($closure);

$future->value();
?>
--EXPECT--
OK
OK
