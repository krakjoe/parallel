--TEST--
Check handlers read property
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

try {
    $parallel->property;
} catch (\parallel\Error $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(50) "parallel\Runtime objects do not support properties"

