--TEST--
Check closures over channel unbuffered
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime;
$channel = \parallel\Channel::make("channel");

$runtime->run(function(){
    $channel = 
        \parallel\Channel::open("channel");
    $closure = $channel->recv();
    $closure();
});

$channel->send(function(){
    echo "OK\n";
});
?>
--EXPECT--
OK


