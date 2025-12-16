--TEST--
Check INIT_FCALL fix with Runtime::run() (undefined function)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
if (ini_get("opcache.enable_cli")) {
	die("skip opcache must not be loaded");
}
?>
--FILE--
<?php
function dummy_func() { return "FOO"; }
$runtime = new \parallel\Runtime(__DIR__ . DIRECTORY_SEPARATOR . 'init_fcall_fix_003_bootstrap.php');
$runtime->run(function(){
	try {
		$s = existing();
        // This will be compiled as INIT_FCALL but should be converted to INIT_FCALL_BY_NAME
		// and fail gracefully because it doesn't exist in the thread.
		$s .= dummy_func();
		$s .= dummy_func();
		return $s();
    } catch (Error $e) {
        echo "Caught: " . $e->getMessage();
    }
})->value();
?>
--EXPECT--
Caught: Call to undefined function dummy_func()
