--TEST--
Missing direct function remains catchable in task
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
function dummy_func(string $value): string { return $value . getenv('DUMMY_VALUE'); }

\parallel\run(function(string $value){
	try {
		return dummy_func($value);
	} catch (Error $e) {
		echo $e->getMessage();
	}
}, ['value'])->value();
?>
--EXPECT--
Call to undefined function dummy_func()
