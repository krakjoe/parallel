--TEST--
parallel recursion in objects
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
\parallel\bootstrap(sprintf("%s/069-bootstrap.inc", __DIR__));

include (sprintf("%s/069-bootstrap.inc", __DIR__));

$foo = new Foo;
$foo->foo = $foo;

\parallel\run(function(Foo $foo){
        var_dump($foo);	
    }, [$foo]);
?>
--EXPECT--
object(Foo)#2 (1) {
  ["foo"]=>
  *RECURSION*
}
