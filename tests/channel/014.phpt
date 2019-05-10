--TEST--
Check Channel comparison
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$lhs = Channel::make("channel");
$rhs = Channel::open("channel");
$ohs = Channel::make("none");

if ($lhs == $ohs) {
    echo "FAIL";
} else if ($lhs == $rhs) {
    echo "OK\n";
    exit;
}

var_dump($lhs, $rhs, $ohs, 
        ($lhs == $ohs), ($lhs == $rhs));
?>
--EXPECT--
OK

