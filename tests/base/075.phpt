--TEST--
Copy packed array return value
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
var_dump((new parallel\Runtime())->run(static fn() => [1.1, 2.2, 3.3, 4])->value());
?>
--EXPECT--
array(4) {
  [0]=>
  float(1.1)
  [1]=>
  float(2.2)
  [2]=>
  float(3.3)
  [3]=>
  int(4)
}
