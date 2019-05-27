--TEST--
parallel cancellation (runtime killed)
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

$parallel->kill();

try {
    $future->cancel();
} catch (\parallel\Future\Error\Killed $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(33) "runtime executing task was killed"
--XLEAK--
The interrupt we use for cancellation is not treated in a thread safe way in core

