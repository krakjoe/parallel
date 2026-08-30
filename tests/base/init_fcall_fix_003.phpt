--TEST--
Direct function call links to Runtime bootstrap
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip parallel extension not loaded';
}
?>
--FILE--
<?php
function dummy_func(string $value): string { return 'submitter-' . $value . getenv('DUMMY_VALUE'); }

$runtime = new \parallel\Runtime(__DIR__ . '/init_fcall_fix_003_bootstrap.php');
echo $runtime->run(function(string $value){
	return dummy_func($value);
}, ['value'])->value();
?>
--EXPECT--
bootstrap-value
