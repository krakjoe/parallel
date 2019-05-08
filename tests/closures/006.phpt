--TEST--
Check closures no nested declarations
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime;
$channel = \parallel\Channel::make("channel", \parallel\Channel::Infinite);

try {
    $channel->send(function(){
        new class {};
    });
} catch (\parallel\Channel\Error\IllegalValue $ex) {
    var_dump($ex->getMessage());
}

try {
    $channel->send(function(){
        class nest {}
    });
} catch (\parallel\Channel\Error\IllegalValue $ex) {
    var_dump($ex->getMessage());
}
?>
--EXPECT--
string(32) "value of type Closure is illegal"
string(32) "value of type Closure is illegal"
