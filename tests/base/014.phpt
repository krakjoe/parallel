--TEST--
ZEND_DECLARE_FUNCTION
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

try {
    $parallel->run(function(){
        function test1() {}
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}

try {
    $parallel->run(function(){
        function () {
            function test2() {

            }
        };
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(48) "illegal instruction (function) on line 1 of task"
string(59) "illegal instruction (function) in closure on line 1 of task"

