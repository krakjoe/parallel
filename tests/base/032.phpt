--TEST--
parallel future saved null
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
	return null;
});

if ($future->value() == null &&
    $future->value() == null) {
	echo "OK";
}

?>
--EXPECT--
OK



