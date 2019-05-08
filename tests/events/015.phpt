--TEST--
Check Events timeout/non-blocking mode
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events;

$events = new Events();
$events->setBlocking(false);

try {
    $events->setTimeout(0);
} catch (\parallel\Events\Error $ex) {
    var_dump($ex->getMessage());
}

$events = new Events();
$events->setTimeout(0);

try {
    $events->setBlocking(false);
} catch (\parallel\Events\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(47) "cannot set timeout on loop in non-blocking mode"
string(45) "cannot set blocking mode on loop with timeout"



