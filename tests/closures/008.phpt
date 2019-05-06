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

$closure = function(){
    static $vars = [0];

    $vars[0]++;

    return $vars[0];
};

for ($i = 0; $i<10; $i++)
    $channel->send($closure);

foreach ($runtimes as $runtime)
    $channel->send(false);

if (($futures[0]->value() + $futures[1]->value()) == 2) {
    echo "OK\n";
    exit;
}

var_dump($futures[0]->value(), $futures[1]->value());
?>
--EXPECT--
OK
