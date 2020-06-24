--TEST--
parallel immutable class load
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
if (!version_compare(PHP_VERSION, "7.4", ">=")) {
    die("skip php 7.4 required");
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