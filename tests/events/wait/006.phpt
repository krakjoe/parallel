--TEST--
Check Events non-blocking iterator exception
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
    foreach ($events as $event) {}
} catch (\parallel\Events\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(50) "cannot create iterator for non-blocking event loop"


