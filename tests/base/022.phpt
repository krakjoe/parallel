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

$future = $parallel->run(function(){
	return new stdClass;
});

var_dump($future->value());
?>
--EXPECTF--
object(stdClass)#%d (0) {
}

