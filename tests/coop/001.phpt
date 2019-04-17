--TEST--
Check basic coop
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

$parallel->run(function(){
    global $f;
    
    while ($f++<5) {
        echo "first\n";
        
        Runtime::yield();
    }
});

$parallel->run(function(){
    global $s;

    while ($s++<5) {
        echo "second\n";
        
        Runtime::yield();
    }
});
?>
--EXPECT--
first
second
first
second
first
second
first
second
first
second


