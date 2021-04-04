--TEST--
parallel check type list missing definition
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
class Foo{}

try {
    \parallel\run(function(int|string|Foo|Bar $a){
        echo "FAIL\n";
    }, [42]);
} catch (parallel\Runtime\Error\IllegalParameter) {
    echo "OK\n";
}
?>
--EXPECT--
OK
