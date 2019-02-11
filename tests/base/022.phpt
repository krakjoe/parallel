--TEST--
return destroyed
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
	return new stdClass;
});

echo "OK";
?>
--EXPECT--
OK

