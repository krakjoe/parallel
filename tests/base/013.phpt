--TEST--
ZEND_DECLARE_LAMBDA_FUNCTION (OK)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new parallel\Runtime();

$runtime->run(function() {
	$closure = function() {
	    return true;
	};
	var_dump($closure());
});

$runtime->run(function() {
	$closure = function() {
	    $result = function(){
	        return true;
	    };
	    return $result();
	};
	var_dump($closure());
});
?>
--EXPECT--
bool(true)
bool(true)


