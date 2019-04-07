--TEST--
Check basic coop new stack
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
    var_dump($a, $b);
    
    if ($a + $b < 10) {
        Runtime::yield([$a+1, $b+1]);
    }
}, [1, 2]);
?>
--EXPECT--
int(1)
int(2)
int(2)
int(3)
int(3)
int(4)
int(4)
int(5)
int(5)
int(6)
