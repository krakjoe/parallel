--TEST--
Check parallel global scope
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$parallel->run(function() {
	global $thing;

	$thing = 10;
});

$parallel->run(function() {
	global $thing;

	var_dump($thing);
});

var_dump(@$thing);
?>
--EXPECT--
int(10)
NULL
