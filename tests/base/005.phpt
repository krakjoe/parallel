--TEST--
Check parallel ini
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
ini_set("include_path", "/none_for_the_purposes_of_this_test");

$parallel = new parallel\Runtime();

$parallel->run(function() {
	if (ini_get("include_path") != "/none_for_the_purposes_of_this_test") {
	    echo "OK";
	}
});
?>
--EXPECTF--
OK
