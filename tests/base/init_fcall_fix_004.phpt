--TEST--
Missing functions in unexecuted task paths do not fail task
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
function direct_func(string $value): string { return $value . getenv('DIRECT_VALUE'); }
function nested_func(string $value): string { return $value . getenv('NESTED_VALUE'); }

$runtime = new \parallel\Runtime();
echo $runtime->run(function(string $value, bool $call){
	$nested = static fn(string $value) => nested_func($value);
	if ($call) {
		return direct_func($value) . $nested($value);
	}
	return 'ok';
}, ['value', false])->value();
?>
--EXPECT--
ok
