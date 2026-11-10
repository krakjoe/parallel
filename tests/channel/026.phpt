--TEST--
Check Channel rejects objects with incompatible property layouts
--DESCRIPTION--
The sender runtime defines Message with three declared properties, the
receiver with one. The receiving copy allocates the object for the receiver
layout, but copies as many properties as the sender declared, writing past
the end of the object. Receiving must reject the incompatible layout.
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
use parallel\{Channel, Runtime};

class Message {
    public $a;
}

$channel = new Channel(1);
$runtime = new Runtime(__DIR__ . '/026.inc');

$runtime->run(static function (Channel $channel): void {
    $channel->send(new Message('a', 'b', 'c'));
}, [$channel]);

$runtime->close();

try {
    $message = $channel->recv();
    var_dump($message);
} catch (\parallel\Channel\Error\IllegalValue $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

echo "OK\n";
?>
--EXPECT--
parallel\Channel\Error\IllegalValue: cannot receive object of class Message, sending and receiving runtimes declare it differently
OK
