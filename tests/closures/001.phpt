--TEST--
Check closures in arginfo/argv (OK)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime;
$runtime->run(function(Closure $arg){
    var_dump($arg());
}, [function(){
    return true;
}]);
?>
--EXPECT--
bool(true)

