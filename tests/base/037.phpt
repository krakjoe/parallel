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
	
	return $foo->bar([42], new stdClass);
});

var_dump($future->value());
?>
--EXPECTF--
Fatal error: Uncaught RuntimeException: message in %s:12
Stack trace:
#0 %sbootstrap.inc(19): Qux->method('array(1)', 'Object(stdClass...')
#1 %s037.php(7): Foo->bar('array(1)', 'Object(stdClass...')
#2 {main}
  thrown in %sbootstrap.inc on line 12
