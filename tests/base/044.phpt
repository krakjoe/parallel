--TEST--
parallel copy persistent repeat
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

$closure = function(){
    return true;
};

$future[] = $parallel->run($closure);
$future[] = $parallel->run($closure);

foreach ($future as $f)
    var_dump($f->value());
?>
--EXPECT--
bool(true)
bool(true)



