--TEST--
Check events add non-future
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$events = new \parallel\Events();

try {
    $events->addFuture("future", new stdClass);
} catch (\parallel\Error\InvalidArguments $ex) {
    var_dump($ex->getMessage());
}

$parallel = new \parallel\Runtime();
$future = $parallel->run(function(){
    return true;
});

$events->addFuture("future", $future);

try {
    $events->addFuture("future", $future);
} catch (\parallel\Events\Error\Existence $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(41) "expected target name and \parallel\Future"
string(35) "target named "future" already added"



