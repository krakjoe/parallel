--TEST--
Check parallel return values
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$future = $parallel->run(function() {
	return 10;
});

var_dump($future->value());
?>
--EXPECT--
int(10)
