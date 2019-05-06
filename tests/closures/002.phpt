--TEST--
Check closures in return
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime;

$future = $runtime->run(function(Closure $closure) : Closure {
    return $closure;
}, [function(){
    return true;
}]);

var_dump(($future->value())());
?>
--EXPECT--
bool(true)


