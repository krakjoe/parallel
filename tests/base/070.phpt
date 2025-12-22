--TEST--
SAPI globals
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
var_dump($_SERVER['argc']);
\parallel\run(function(){
    var_dump($_SERVER['argc']);
});
?>
--EXPECT--
int(1)
int(1)
