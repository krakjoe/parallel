--TEST--
Check functional bootstrap error (set in task)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$future = \parallel\run(function() {
    \parallel\bootstrap("1.php");
});

try {
    $future->value();
} catch (\parallel\Runtime\Error\Bootstrap $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(76) "\parallel\bootstrap should be called once, before any calls to \parallel\run"

