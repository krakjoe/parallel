--TEST--
Check functional bootstrap 
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
\parallel\bootstrap(
    sprintf("%s/002.inc", __DIR__));
\parallel\run(function(){
	foo();
});
?>
--EXPECT--
OK
