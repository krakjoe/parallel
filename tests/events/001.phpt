--TEST--
Check Events setInput overwrite
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events\Input;
use \parallel\Events;

$events = new Events();
$events->setInput(new Input);
$events->setInput(new Input);

echo "OK";
?>
--EXPECT--
OK


