--TEST--
bailed
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime(sprintf("%s/bootstrap.inc", __DIR__));

try {
	$parallel->run(function(){
		thrower();
	});
} catch (Error $er) { 
	/* can't catch here what is thrown in runtime */
}
?>
--EXPECTF--
Fatal error: Uncaught %s in %s:7
Stack trace:
#0 %s(6): thrower()
#1 {main}
  thrown in %s on line 7

