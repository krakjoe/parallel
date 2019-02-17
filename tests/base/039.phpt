--TEST--
parallel Future::select all errors
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$i = 0;
$futures = [];

while ($i++ < 5) {
	$futures[$i] = $parallel->run(function($throw){
		if ($throw) {
			throw new Exception();
		}
		return true;
	}, [true]);
}

$resolved = [];
$errored  = [];

$storedResolutions = [];
$storedErrors      = [];

while (\parallel\Future::select($futures, $resolved, $errored)) {
	$storedResolutions = array_merge($storedResolutions, $resolved);
	$storedErrors      = array_merge($storedErrors, $errored);
}

var_dump(count($storedResolutions), count($storedErrors));
?>
--EXPECTF--
Fatal error: Uncaught Exception in %s:10
Stack trace:
#0 [internal function]: \parallel\Runtime::run(true)
#1 {main}
  thrown in %s on line 10

Fatal error: Uncaught Exception in %s:10
Stack trace:
#0 [internal function]: \parallel\Runtime::run(true)
#1 {main}
  thrown in %s on line 10

Fatal error: Uncaught Exception in %s:10
Stack trace:
#0 [internal function]: \parallel\Runtime::run(true)
#1 {main}
  thrown in %s on line 10

Fatal error: Uncaught Exception in %s:10
Stack trace:
#0 [internal function]: \parallel\Runtime::run(true)
#1 {main}
  thrown in %s on line 10

Fatal error: Uncaught Exception in %s:10
Stack trace:
#0 [internal function]: \parallel\Runtime::run(true)
#1 {main}
  thrown in %s on line 10
int(0)
int(5)
