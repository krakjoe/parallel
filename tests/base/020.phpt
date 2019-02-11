--TEST--
Copy arginfo (OK)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$future[0] = $parallel->run(function(int $int){
	return $int;
}, [42]);
var_dump($future[0]->value());

$future[1] = $parallel->run(function(float $dbl){
	return $dbl;
}, [4.2]);
var_dump($future[1]->value());

$future[2] = $parallel->run(function(string $str){
	return $str;
}, ["four"]);
var_dump($future[2]->value());

$future[3] = $parallel->run(function(string $str) : string {
	return $str;
}, ["to"]);
var_dump($future[3]->value());

$future[4] = $parallel->run(function(string ... $str) : string {
	return sprintf("%s %s", $str[0], $str[1]);
}, ["fourty", "two"]);
var_dump($future[4]->value());
?>
--EXPECT--
int(42)
float(4.2)
string(4) "four"
string(2) "to"
string(10) "fourty two"

