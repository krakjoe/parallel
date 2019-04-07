--TEST--
Check basic coop stack
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Runtime;

$parallel = new Runtime();

$parallel->run(function($a, $b){
    global $f;
    
    var_dump($a, $b);
    
    if ($f++<5) {
        Runtime::yield();
    }
}, ["Hello", "World"]);
?>
--EXPECT--
string(5) "Hello"
string(5) "World"
string(5) "Hello"
string(5) "World"
string(5) "Hello"
string(5) "World"
string(5) "Hello"
string(5) "World"
string(5) "Hello"
string(5) "World"
string(5) "Hello"
string(5) "World"


