--TEST--
Check Channel constructor
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

try {
    new Channel;
} catch (\parallel\Channel\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(46) "construction of Channel objects is not allowed"



