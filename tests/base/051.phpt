--TEST--
parallel l2 cache copies
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}

if (ini_get("opcache.enable_cli")) {
    die("skip opcache must not be enabled");
}
?>
--FILE--
<?php
use \parallel\{Runtime, Channel};

$runtime = new Runtime;
$channel = Channel::make("channel");

$future = $runtime->run(function(){
    /*
    * When the l2 cache stores this function to return it to the main thread,
    * it must make a deep copy because when this runtime is shutdown
    * it will destroy the original.
    *
    * The l2 cache makes a full deep copy of everything it caches, interning all
    * strings, and making arrays immutable, so that code from the l2 cache is as
    * safe as code from opcache.
    *
    * None of this is applicable when only the l1 (copy) cache is being used.
    */
    return include(sprintf("%s/051.inc", __DIR__));
});

$runtime->close();

$closure = $future->value();

$reflection = new ReflectionFunction($closure);

$params = $reflection->getParameters();

var_dump($closure(new stdClass), $params[0]->getName());
?>
--EXPECTF--
object(stdClass)#%d (%d) {
}
string(3) "std"



