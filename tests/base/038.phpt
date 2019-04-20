--TEST--
parallel Future value refcounted unfetched
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
	return [42];
});

$parallel->close();

/* this test will leak if dtor is incorrect, cannot fetch future value */
echo "OK";
?>
--EXPECT--
OK

