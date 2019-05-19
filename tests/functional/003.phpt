--TEST--
Check functional future 
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
\parallel\bootstrap(
    sprintf("%s/003.inc", __DIR__));

$future = \parallel\run(function(){
	return foo();
});

var_dump($future->value());
?>
--EXPECT--
string(2) "OK"
