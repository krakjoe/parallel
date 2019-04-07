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
    
    echo "first\n";
    
    if ($f++<5) {
        Runtime::yield();
    }
});

$parallel->run(function(){
    global $s;

    echo "second\n";
    
    if ($s++<5) {
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
first
second

