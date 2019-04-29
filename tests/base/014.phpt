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

$parallel->run(function() {
	function test() {
	    return true;
	}
	
	var_dump(test());
});

$parallel->run(function() {
	function test2() {
	    function test3() {
	        return true;
	    }
	    return test3();
	}
	
	var_dump(test2());
});
?>
--EXPECT--
bool(true)
bool(true)

