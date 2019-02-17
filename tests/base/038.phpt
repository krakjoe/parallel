--TEST--
parallel Future::select no errors
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$i = 0;
$workers = [];

while ($i++ < 5) {
	$workers[$i] = new \parallel\Runtime();
}

$i = 0;
$futures = [];

while ($i++ < 5) {
	$futures[$i] = $workers[$i]->run(function(){
		return true;
	});
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
--EXPECT--
int(5)
int(0)
