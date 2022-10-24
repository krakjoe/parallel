--TEST--
parallel check type list property
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
?>
--FILE--
<?php
\parallel\bootstrap(sprintf("%s/066-bootstrap.inc", __DIR__));

include (sprintf("%s/066-bootstrap.inc", __DIR__));

\parallel\run(function(Foo $a){
        echo "OK\n";	
    }, [new Foo()]);
?>
--EXPECT--
OK
