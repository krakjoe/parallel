--TEST--
Check functional reuse 
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$future = \parallel\run(function() {
    global $var;

    $var = 42;
});

$future->value(); # we know that the
                  # runtime started for the first
                  # task is free, the next task
                  # must reuse the runtime

\parallel\run(function(){
    global $var;

    var_dump($var);
});
?>
--EXPECT--
int(42)
