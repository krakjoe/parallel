--TEST--
ZEND_BIND_STATIC (FAIL)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();
$var     = null; /* avoid undefined */

try {
	$parallel->run(function() use($var) {});
} catch (Error $ex) {
	var_dump($ex->getMessage());
}
?>
--EXPECT--
string(44) "illegal instruction (lexical) in entry point"


