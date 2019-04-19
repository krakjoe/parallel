--TEST--
parallel Future value timeout
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$future = $parallel->run(function(){
	while (true) {
		usleep(1000);
	}
	return;
});

try {
	$future->value(100000);
} catch (\parallel\Future\Timeout $ex) {
	var_dump($ex->getMessage());
}

$parallel->kill();
?>
--EXPECT--
string(48) "a timeout occured waiting for value from Runtime"







