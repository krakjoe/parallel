--TEST--
parallel nested runtime output during startup and shutdown
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    die('skip');
}
?>
--FILE--
<?php
$bootstrap = __DIR__ . '/076-bootstrap.inc';
for ($i = 0; $i < 2; $i++) {
    echo "root start\n";
    $runtime = new parallel\Runtime($bootstrap);
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
root end
root start
bootstrap
child
bootstrap
grandchild
shutdown
child end
shutdown
root end
