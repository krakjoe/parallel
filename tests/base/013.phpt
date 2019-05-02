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

$f2 = $runtime->run(function() {
	$closure = function() {
	    $result = function(){
	        return true;
	    };
	    return $result();
	};
	var_dump($closure());
});

$f1->value() && $f2->value();

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
--EXPECTF--
bool(true)
bool(true)
string(%d) "illegal instruction (new class) on line 1 of closure declared on line 26 of %s and scheduled"
string(%d) "illegal instruction (class) on line 1 of closure declared on line 38 of %s and scheduled"




