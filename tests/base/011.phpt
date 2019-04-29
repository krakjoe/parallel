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
$array = [42];

$future = $parallel->run(function() use($array) {
	static $var;

	$var++;
	
	var_dump($array);
	
	$array[] = 42;
	
	var_dump($array);

	return $var;
});

var_dump($future->value());

var_dump($var, $array);
?>
--EXPECT--
array(1) {
  [0]=>
  int(42)
}
array(2) {
  [0]=>
  int(42)
  [1]=>
  int(42)
}
int(1)
int(10)
array(1) {
  [0]=>
  int(42)
}


