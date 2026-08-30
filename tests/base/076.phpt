--TEST--
JIT compiles code executed in a worker
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip parallel extension not loaded';
}
$status = function_exists('opcache_get_status') ? opcache_get_status(false) : false;
if (!$status || !($status['jit']['on'] ?? false)) {
	echo 'skip JIT not active';
}
?>
--INI--
opcache.enable_cli=1
opcache.jit=tracing
opcache.jit_buffer_size=64M
opcache.file_update_protection=0
opcache.protect_memory=0
--FILE--
<?php
$result = parallel\run(static function (): array {
	$before = opcache_get_status(false)['jit']['buffer_free'];
	$sum = 0;
	for ($i = 0; $i < 1000000; $i++) {
		$sum += $i % 7;
	}
	$after = opcache_get_status(false)['jit']['buffer_free'];
	return [$sum, $after < $before];
})->value();

var_dump(...$result);
?>
--EXPECT--
int(2999997)
bool(true)
