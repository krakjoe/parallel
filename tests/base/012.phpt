--TEST--
ZEND_BIND_STATIC (FAIL)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--INI--
opcache.optimization_level=0
--FILE--
<?php
$parallel = new parallel\Runtime();
$var     = null; /* avoid undefined */

try {
	$parallel->run(function() use(&$var) {});
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECTF--
string(%d) "illegal instruction (lexical reference) in task"


