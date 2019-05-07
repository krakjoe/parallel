--TEST--
parallel auto globals
--SKIPIF--
<?php
if (!extension_loaded('parallel') || strtoupper(substr(PHP_OS, 0, 3)) == 'WIN') {
	die("skip non windows test");
}
?>
--INI--
variables_order=EGPCS
--FILE--
<?php
$parallel = new \parallel\Runtime();

$parallel->run(function(){
	if (count($_SERVER) > 0) {
		echo "SERVER\n";
	}
	
	if (count($_ENV) > 0) {
		echo "ENV\n";
	}
});

$parallel = new \parallel\Runtime();

$parallel->run(function(){
    $closure = function() {
        return $_SERVER;
    };
    
    if (count($closure()) > 0) {
        echo "NESTED SERVER\n";
    }
});
?>
--EXPECT--
SERVER
ENV
NESTED SERVER
