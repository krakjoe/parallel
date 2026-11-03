--TEST--
Issue #413: OPcache status remains valid in parallel runtimes
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    die("skip parallel not loaded");
}
if (!extension_loaded('Zend OPcache')) {
    die("skip OPcache not loaded");
}
/* Expected fix releases for php/php-src#22225; adjust if delayed. */
if ((PHP_VERSION_ID >= 80500 && version_compare(PHP_VERSION, "8.5.13", "<"))
    || (PHP_VERSION_ID === 80600 && version_compare(PHP_VERSION, "8.6.0RC5", "<"))) {
    die("xfail PHP GH-22216: incorrect OPcache memory statistics in new threads");
}
?>
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.log_verbosity_level=1
--FILE--
<?php
$check = static function () {
    $status = opcache_get_status(false);
    $memory = $status['memory_usage'];

    var_dump($status['opcache_enabled']);
    var_dump($memory['used_memory'] >= 0);
    var_dump(
        $memory['used_memory'] + $memory['free_memory'] + $memory['wasted_memory']
        === (int) ini_get('opcache.memory_consumption') * 1024 * 1024
    );
    var_dump(is_finite($memory['current_wasted_percentage']));
};

echo "main\n";
$check();

echo "worker\n";
$runtime = new parallel\Runtime();
$runtime->run($check);
$runtime->close();
?>
--EXPECT--
main
bool(true)
bool(true)
bool(true)
bool(true)
worker
bool(true)
bool(true)
bool(true)
bool(true)
