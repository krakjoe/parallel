--TEST--
parallel may return arrays
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime();

$future = $parallel->run(function(){
	$stdin = fopen("php://input", "r");

	var_dump($stdin);

	return [
		1, 2, 3,
		[
			4, 5, 6
		],
		"seven" => new stdClass,
		"eight" => "string here",
		"stdin" => $stdin
	];
});

var_dump($future->value());
?>
--EXPECTF--
resource(%d) of type (stream)
array(7) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
  }
  ["seven"]=>
  object(stdClass)#%d (%d) {
  }
  ["eight"]=>
  string(11) "string here"
  ["stdin"]=>
  NULL
}

