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
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}

$input->remove("test");

try {
    $input->remove("test");
} catch (\parallel\Exception $ex) {
    var_dump($ex->getMessage());
}

$input->add("test", "thing");
$input->clear();
$input->add("test", "thing");

echo "OK";
?>
--EXPECT--
string(23) "payload for test exists"
string(31) "payload for test does not exist"
OK
