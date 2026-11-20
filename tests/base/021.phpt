--TEST--
Copy try
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--INI--
error_reporting=E_ALL & ~E_DEPRECATED
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

