--TEST--
parallel immutable class load
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
declare(strict_types = 1);

use parallel\Runtime;

$bootstrapFile = sprintf('%s/062.bootstrap.inc', __DIR__);
include_once $bootstrapFile;

$rt = new Runtime($bootstrapFile);
$params = new EnvWrap(new EnvDto('ok'));

$rt->run(static function (EnvWrap $params) {
    echo $params->getEnv()->getName();
}, [$params]);
?>
--EXPECT--
ok