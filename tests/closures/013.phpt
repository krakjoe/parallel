--TEST--
Check closure cache does not return stale cached closure after file change
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
if (PHP_VERSION_ID < 80400) {
	echo 'skip - requires PHP 8.4+ for unique closure names';
}
?>
--FILE--
<?php
$f = tempnam(sys_get_temp_dir(), 'par_') . '.php';

foreach (['FIRST', 'SECOND'] as $val) {
	file_put_contents($f, "<?php return function() { return '$val'; };\n");
	if (function_exists('opcache_invalidate')) {
		opcache_invalidate($f, true);
	}
	$r = new \parallel\Runtime();
	$result = $r->run(include $f)->value();
	$r->close();
	echo "$result\n";
}

unlink($f);
?>
--EXPECT--
FIRST
SECOND
