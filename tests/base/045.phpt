--TEST--
parallel cancellation (not running)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$parallel->run(function(){
    while(1)
        usleep(10000);
});

$future = $parallel->run(function(){
    echo "FAIL\n";
});

var_dump($future->cancel(), $future->cancelled());

$parallel->kill();
?>
--EXPECT--
bool(true)
bool(true)
--XLEAK--
The interrupt we use for cancellation is not treated in a thread safe way in core



