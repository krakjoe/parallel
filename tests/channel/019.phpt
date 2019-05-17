--TEST--
Check Channel closures inside object properties (declared)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Runtime, Channel};

include sprintf("%s/019.inc", __DIR__);

$runtime = new Runtime(sprintf("%s/019.inc", __DIR__));
$channel = Channel::make("channel");

$runtime->run(function($channel){
    $foo = $channel->recv();

    $foo->call();
}, [$channel]);

$foo = new Foo(function(){
    echo "OK";
});

$channel->send($foo);
?>
--EXPECT--
OK

