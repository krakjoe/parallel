--TEST--
parallel: static variables in functions are isolated
--FILE--
<?php
function b() {
	static $i = 0;
	$i++;
	sleep(1);
	return $i;
}

function a() {
    return b();
}

$futures[] = \parallel\run(function() { return a(); });
$futures[] = \parallel\run(function() { return a(); });
$futures[] = \parallel\run(function() { return a(); });
$futures[] = \parallel\run(function() { return a(); });

foreach ($futures as $future) {
	echo "Done: ".$future->value().PHP_EOL;
}
?>
--EXPECT--
Done: 1
Done: 1
Done: 1
Done: 1

