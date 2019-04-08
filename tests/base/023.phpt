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
$parallel = new parallel\Runtime();

try {
	$parallel->run(function(){
		throw new Error();
	});
} catch (Error $er) { 
	/* can't catch here what is thrown in parallel */
}
?>
--EXPECTF--
Fatal error: Uncaught Error in %s:6
Stack trace:
#0 {main}
  thrown in %s on line 6

