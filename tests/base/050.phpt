--TEST--
parallel cancellation (value on cancelled)
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

$sync->send(true);

$future->cancel();

try {
    $future->value();
} catch (\parallel\Future\Error\Cancelled $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(21) "cannot retrieve value"
--XLEAK--
The interrupt we use for cancellation is not treated in a thread safe way in core


