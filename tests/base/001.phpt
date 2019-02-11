--TEST--
Check basic parallel operation
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$parallel->run(function(){
	echo "OK";
});
?>
--EXPECT--
OK
