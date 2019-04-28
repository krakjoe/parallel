--TEST--
Check Events Input invalid value
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events\Input;

$input = new Input();

try {
    $input->add("target", new stdClass);
} catch (\parallel\Events\Input\Error\IllegalValue $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(31) "value of type object is illegal"


