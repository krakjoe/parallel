--TEST--
Include before direct function call resolves in task
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
function dummy_func(string $value): string { return 'submitter-' . $value . getenv('DUMMY_VALUE'); }

$runtime = new \parallel\Runtime();
echo $runtime->run(function(string $value){
	include __DIR__ . '/init_fcall_fix_002_include.inc';
	return dummy_func($value);
}, ['value'])->value();
?>
--EXPECT--
worker-value
