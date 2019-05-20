--TEST--
Check functional bootstrap error (set after run)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$sync = \parallel\channel::make("sync");
\parallel\run(function() use($sync) {
    $sync->recv();
});

try {
    \parallel\bootstrap("1.php");
} catch (\parallel\Runtime\Error\Bootstrap $ex) {
    var_dump($ex->getMessage());
}

$sync->send(true);
?>
--EXPECT--
string(76) "\parallel\bootstrap should be called once, before any calls to \parallel\run"

