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

$channel = Channel::make("channel");

$thread = function(){
    $channel = Channel::open("channel");

    while (($closure = $channel->recv())) {
        if ($closure() != 1) {
            echo "FAIL\n";
        }
    }
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

echo "OK";
?>
--EXPECT--
OK
