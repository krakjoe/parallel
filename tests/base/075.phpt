--TEST--
Check function copying with opcache enabled
--EXTENSIONS--
opcache
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    die('skip parallel not loaded');
}
?>
--INI--
opcache.enable=1
opcache.enable_cli=1
--FILE--
<?php
function b() { echo "done."; }
function a() { b(); }
(new \parallel\Runtime)->run(function() { a(); });
?>
--EXPECT--
done.
