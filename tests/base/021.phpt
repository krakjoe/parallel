--TEST--
Copy try
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
	try {
		throw Error();
	} catch(Error $ex) {
		echo "OK\n";
	} finally {
		return true;
	}
});

var_dump($future->value());
?>
--EXPECT--
OK
bool(true)

