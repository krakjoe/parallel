--TEST--
ZEND_DECLARE_INHERITED_CLASS
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
		class Foo extends Bar {}
	});
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal instruction (class) on line 1 of task"



