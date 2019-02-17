--TEST--
parallel Future::select timeouts
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
		while (true) {
			usleep(100);
		}
		return true;
	}, [true]);
}

$resolved = [];
$errored  = [];
$timedout = [];

while (\parallel\Future::select($futures, $resolved, $errored, $timedout, 500000)) {
	var_dump(count($resolved), count($timedout));
	break;
}

$parallel->kill();
?>
--EXPECTF--
int(0)
int(5)
