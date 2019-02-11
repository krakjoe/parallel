--TEST--
ZEND_BIND_STATIC (OK)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$var = 10;

$future = $parallel->run(function(){
	static $var;

	$var++;

	return $var;
});

var_dump($future->value());

var_dump($var);
?>
--EXPECT--
int(1)
int(10)
