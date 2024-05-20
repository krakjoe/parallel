--TEST--
parallel recursion in arrays
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$array = [];
$array["self"] = &$array;

\parallel\run(function($array){
        var_dump($array);
    }, [$array]);
?>
--EXPECT--
array(1) {
  ["self"]=>
  *RECURSION*
}
--XFAIL--
REASON: no cyclic reference collector implemented yet
