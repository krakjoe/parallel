--TEST--
parallel runtime copy
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime();

try {
    \parallel\run(function() use($runtime){
        $runtime->run(function(){ 
            echo "hi\n"; 
        });
    });
} catch (\parallel\Runtime\Error\IllegalVariable $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(77) "illegal variable (parallel\Runtime) named runtime in static scope of function"


