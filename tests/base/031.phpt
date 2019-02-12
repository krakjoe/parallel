--TEST--
parallel future saved
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();
$future   = $parallel->run(function(){
	return 42;
});

if ($future->value() == 42 &&
    $future->value() == 42) {
	echo "OK";
}

?>
--EXPECT--
OK



