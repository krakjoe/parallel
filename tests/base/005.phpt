--TEST--
Check parallel configuration
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new parallel\Runtime([
	"include_path" => "dummy",
	"cast_to_string" => 2.2
]);

$parallel->run(function() {
	var_dump(ini_get("include_path"));
});
?>
--EXPECTF--
string(%d) "dummy"
