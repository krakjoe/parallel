--TEST--
parallel\Runtime::run() - closure calls undefined function in worker thread
--DESCRIPTION--
Fixes https://github.com/krakjoe/parallel/issues/317
If you call a function that exists in the main thread's `EG(function_table)`
from a `\parallel\Runtime`, this will compile just fine, as PHP is not aware
of the execution in the other thread, but the execution will segfault. This
test ensures that instead of segfaulting this throws an Exception to the
caller that can be catched.
--EXTENSIONS--
parallel
--SKIPIF--
<?php
if (ini_get("opcache.enable_cli")) {
	die("skip opcache must not be loaded");
}
if (version_compare(PHP_VERSION, "8.2.0", "<")) {
    die("skip php 8.1 is failing on its own gracefully");
}
?>
--FILE--
<?php
function a()
{
	return "foo";
}

$runtime = new parallel\Runtime();
try {
	$future = $runtime->run(function () {
		return a();
	});
	$future->value();
} catch (parallel\Runtime\Error\IllegalInstruction $e) {
	var_dump($e->getMessage());
	var_dump($e->getFile());
	var_dump($e->getLine());
}
$future = $runtime->run(function () {
	return "done";
});
var_dump($future->value());
?>
--EXPECTF--
string(%d) "Call to undefined function a() in task, please provide the function via a bootstrap file"
string(%d) "%s073.php"
int(10)
string(4) "done"
