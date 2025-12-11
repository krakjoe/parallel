--TEST--
Check function copying without opcache
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    die('skip parallel not loaded');
}
if (ini_get("opcache.enable_cli")) {
    die("skip opcache must not be loaded for this test");
}
?>
--FILE--
<?php
function b() { echo "done."; }
function a() { b(); }
(new \parallel\Runtime)->run(function() { a(); });
?>
--EXPECT--
done.
