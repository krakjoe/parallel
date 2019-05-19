--TEST--
Check functional parallel operation
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
\parallel\run(function(){
	echo "OK";
});
?>
--EXPECT--
OK
