--TEST--
Check array literal copying into threads
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$run = function() {
    $a = [];
    for ($i = 0; $i < 100000; $i++) {
        $b = &$a[$i];
    }
    return 0;
};

for ($i = 0; $i < 10; $i++) {
    $futures[$i] = \parallel\run($run, []);
}

for ($i = 0; $i < 10; $i++) {
    var_dump($futures[$i]->value());
}
?>
--EXPECT--
int(0)
int(0)
int(0)
int(0)
int(0)
int(0)
int(0)
int(0)
int(0)
int(0)
