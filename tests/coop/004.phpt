--TEST--
Check basic coop restricted yield
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Runtime;

$parallel = new Runtime(sprintf("%s/coop.inc", __DIR__));

$parallel->run(function(){
    try {
        doYieldFromFunction();
    } catch (Exception $e) {
        var_dump($e->getMessage());
    }
});

?>
--EXPECT--
string(22) "cannot yield from here"

