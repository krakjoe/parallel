--TEST--
parallel bootstrap
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
	return bootstrapped();
});

var_dump($future->value());
?>
--EXPECT--
bool(true)

