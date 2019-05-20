--TEST--
ZEND_DECLARE_LAMBDA_FUNCTION
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--INI--
opcache.optimization_level=0
--FILE--
<?php
$runtime = new parallel\Runtime();

$f1 = $runtime->run(function() {
	$closure = function() {
	    return true;
	};
	var_dump($closure());
});

$f1->value();

$f2 = $runtime->run(function() {
	$closure = function() {
	    $result = function(){
	        return true;
	    };
	    return $result();
	};
	var_dump($closure());
});

$f2->value();

try {
    $runtime->run(function() {
        $closure = function() {
            $closure = function() {
                new class{};
            };
        };
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}

try {
    $runtime->run(function() {
        $closure = function() {
            $closure = function() {
                class illegal {}
            };
        };
    });
} catch (\parallel\Runtime\Error\IllegalInstruction $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
bool(true)
bool(true)
string(60) "illegal instruction (new class) in closure on line 2 of task"
string(56) "illegal instruction (class) in closure on line 2 of task"





