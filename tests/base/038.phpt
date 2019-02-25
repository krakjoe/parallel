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
		return 'x';
	});
}

$resolved = [];
$errored  = [];

$storedResolutions = [];
$storedErrors      = [];
$result = '';

while (\parallel\Future::select($futures, $resolved, $errored)) {
	foreach ($resolved as $r) {
		$result .= $r->value();
	}
	$storedResolutions = array_merge($storedResolutions, $resolved);
	$storedErrors      = array_merge($storedErrors, $errored);
}

var_dump(count($storedResolutions), count($storedErrors), $result);
?>
--EXPECT--
int(5)
int(0)
string(5) "xxxxx"
