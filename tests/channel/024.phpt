--TEST--
Check buffered Channel receives objects after the sender Runtime closes
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
use parallel\{Channel, Runtime};

require __DIR__ . '/024.inc';

foreach ([1, Channel::Infinite] as $capacity) {
    $channel = new Channel($capacity);
    $runtime = new Runtime(__DIR__ . '/024.inc');

    $runtime->run(static function (Channel $channel): void {
        $channel->send(new ChannelMessage('hello'));
    }, [$channel]);

    $runtime->close();

    $message = $channel->recv();
    var_dump($message instanceof ChannelMessage, $message->value);
}
?>
--EXPECT--
bool(true)
string(5) "hello"
bool(true)
string(5) "hello"
