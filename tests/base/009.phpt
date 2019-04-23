--TEST--
ZEND_DECLARE_ANON_CLASS
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
		new class {};
	});
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal instruction (new class) on line 1 of task"



