--TEST--
parallel nested and idle runtime output during startup and shutdown
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    die('skip');
}
if (PHP_VERSION_ID < 80300 && function_exists('opcache_get_status')
    && (opcache_get_status(false)['jit']['on'] ?? false)) {
    die('skip PHP <= 8.2 JIT can reuse freed internal-function pointers after worker shutdown');
}
?>
--FILE--
<?php
$bootstrap = __DIR__ . '/076-bootstrap.inc';
for ($i = 0; $i < 2; $i++) {
    echo "root start\n";
    $runtime = new parallel\Runtime($bootstrap);
    $idle = new parallel\Runtime();
    $future = $runtime->run(static function ($bootstrap) {
        echo "child\n";
        $nested = new parallel\Runtime($bootstrap);
        $future = $nested->run(static function () {
            echo "grandchild\n";
        });
        $future->value();
        $nested->close();
        echo "child end\n";
    }, [$bootstrap]);
    $future->value();
    unset($future, $runtime);
    echo "root with idle\n";
    $idle->kill();
    unset($idle);
    echo "root end\n";
}
?>
--EXPECT--
root start
bootstrap
child
bootstrap
grandchild
shutdown
child end
shutdown
root with idle
root end
root start
bootstrap
child
bootstrap
grandchild
shutdown
child end
shutdown
root with idle
root end
