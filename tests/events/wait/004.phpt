--TEST--
Check events wait nothing
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

foreach ($events as $event) {
    echo "FAIL\n";
}

echo "OK\n";
?>
--EXPECT--
OK






