--TEST--
Check parallel closed
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

$parallel->close();

try {
	$parallel->run(function(){});
} catch (\parallel\Runtime\Error\Closed $e) {
	echo "OK\n";
}

try {
	$parallel->close();
} catch (\parallel\Runtime\Error\Closed $e) {
	echo "OK";
}
?>
--EXPECT--
int(10)
OK
OK
