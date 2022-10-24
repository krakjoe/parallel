--TEST--
parallel check type list simple
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
class Foo{}
class Bar{}

\parallel\run(function(int|string|Foo|Bar $a){
	echo "OK\n";
}, [42]);
?>
--EXPECT--
OK
