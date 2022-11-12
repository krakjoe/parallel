--TEST--
GH-205 (segmentation fault when creating a sync class)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$a = [];

for($i = 0; $i < 100; $i++){
	$a[] = new \parallel\Sync(false);
}
?>
--EXPECT--
