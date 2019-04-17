--TEST--
Check basic coop entry
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

$parallel->run(function($a){
    echo "enter\n";
    
    while ($a < 10) {
        var_dump($a);
        
        $a += 1;
        
        Runtime::yield();
    }
    
    echo "leave\n";
    
}, [1]);
?>
--EXPECT--
enter
int(1)
int(2)
int(3)
int(4)
int(5)
int(6)
int(7)
int(8)
int(9)
leave


