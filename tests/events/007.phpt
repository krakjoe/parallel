--TEST--
Check events input
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$input = new \parallel\Events\Input();

$input->add("test", "thing");

try {
    $input->add("test", "thing");
} catch (\parallel\Events\Input\Error\Existence $ex) {
    var_dump($ex->getMessage());
}

$input->remove("test");

try {
    $input->remove("test");
} catch (\parallel\Events\Input\Error\Existence $ex) {
    var_dump($ex->getMessage());
}

$input->add("test", "thing");
$input->clear();
$input->add("test", "thing");

echo "OK";
?>
--EXPECT--
string(28) "input for target test exists"
string(36) "input for target test does not exist"
OK
