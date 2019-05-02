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

$f1 = $parallel->run(function() {
	function test() {
	    return true;
	}
	
	var_dump(test());
});

$f2 = $parallel->run(function() {
	function test2() {
	    function test3() {
	        return true;
	    }
	    return test3();
	}
	
	var_dump(test2());
});

$f1->value() && $f2->value();

try {
    $parallel->run(function(){
        function test3() {
            new class{};
        }
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}

try {
    $parallel->run(function(){
        function test4() {
            class illegal {}
        }
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECTF--
bool(true)
bool(true)
string(%d) "illegal instruction (new class) on line 1 of test3 declared on line 27 of %s and scheduled"
string(%d) "illegal instruction (class) on line 1 of test4 declared on line 37 of %s and scheduled"

