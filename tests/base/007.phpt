--TEST--
Check parallel entry
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
	$parallel->run([]);
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
?>
--EXPECT--
string(37) "Closure, or Closure and args expected"


