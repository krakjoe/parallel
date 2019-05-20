--TEST--
parallel Future exception with trace
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime(sprintf("%s/bootstrap.inc", __DIR__));

$future = $parallel->run(function(){
	$foo = new Foo();
	
	return $foo->bar([42],new stdClass);
});

var_dump($future->value());
?>
--EXPECTF--
Fatal error: Uncaught RuntimeException: message in %s:12
Stack trace:
#0 %s(19): Qux->method(Array, Object(stdClass))
#1 %s(7): Foo->bar(Array, Object(stdClass))
#2 {main}
  thrown in %s on line 12

