--TEST--
parallel cancellation (running)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();
$sync     = \parallel\Channel::make("sync");
$future = $parallel->run(function(){
    $sync = \parallel\Channel::open("sync");
    $sync->recv();

    while(1)
        usleep(10000);
});

$parallel->run(function(){
    $sync = \parallel\Channel::open("sync");
    $sync->recv();

    echo "OK\n";
});

$sync->send(true);

var_dump($future->cancel(), $future->cancelled());

$sync->send(true);
?>
--EXPECT--
bool(true)
bool(true)
OK
--XLEAK--
The interrupt we use for cancellation is not treated in a thread safe way in core



