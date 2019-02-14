--TEST--
parallel can create parallel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$parallel->run(function(){
	try {
		$child = new \parallel\Runtime();

		echo "OK\n";
	} catch (\parallel\Exception $ex) {
		
	}
});
?>
--EXPECT--
OK

