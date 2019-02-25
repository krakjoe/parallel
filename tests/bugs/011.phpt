--TEST--
parallel Future::select value hangs (error)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$futures[] = $parallel->run(function(){
    return 42;
});

$futures[] = $parallel->run(function(){
    throw new Exception();
    
    return true;
});

$resolved = [];
$errored = [];

while (\parallel\Future::select($futures, $resolved, $errored)) {
	var_dump(count($resolved), count($errored));

	if (count($resolved)) {
	    $future = array_shift($resolved);
	    
	    var_dump($future->done());
	    var_dump($future->value());
	}

	if (count($errored)) {
	    $future = array_shift($errored);
	    
	    var_dump($future->done());
	    
	    try {
	        $future->value();
	    } catch (\parallel\Exception $ex) {
	        var_dump($ex->getMessage());
	    }
	}
}
?>
--EXPECTF--
int(1)
int(0)
bool(true)
int(42)

Fatal error: Uncaught Exception in %s:9
Stack trace:
#0 [internal function]: \parallel\Runtime::run()
#1 {main}
  thrown in %s on line 9
int(0)
int(1)
bool(true)
string(54) "an exception occured in Runtime, cannot retrieve value"

