--TEST--
Check functional channel aliases
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$channel = \parallel\make("sync");
$dup     = \parallel\open("sync");

if ($channel == $dup) {
    echo "OK";
}
?>
--EXPECT--
OK
