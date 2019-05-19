--TEST--
Check closures cached function check
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Runtime;

$runtime = new Runtime;

$closure = function() {
    $closure = function() {
        return "OK";
    };

    var_dump($closure());
};

$runtime->run($closure);
$runtime->run($closure);
?>
--EXPECT--
string(2) "OK"
string(2) "OK"



