--TEST--
Check closures statics scopes
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Channel, Runtime};

$runtimes = [
    new Runtime,
    new Runtime
];

$futures = [];

$channel = Channel::make("channel");

$thread = function(){
    $channel = Channel::open("channel");
    $result  = 0;

    while (($closure = $channel->recv())) {
        $result = $closure();
    }

    return $result;
};

$futures[0] = $runtimes[0]->run($thread);
$futures[1] = $runtimes[1]->run($thread);

for ($i = 0; $i<10; $i++)
$channel->send(function(){
    static $vars = [0];

    $vars[0]++;

    return $vars[0];
});

foreach ($runtimes as $runtime)
    $channel->send(false);

if (($futures[0]->value() + $futures[1]->value()) == 10) {
    echo "OK\n";
}
?>
--EXPECT--
OK
