--TEST--
Check buffered Channel discards objects after the sender Runtime closes
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
use parallel\{Channel, Runtime};

foreach ([1, Channel::Infinite] as $capacity) {
    $channel = new Channel($capacity);
    $runtime = new Runtime(__DIR__ . '/024.inc');

    $runtime->run(static function (Channel $channel): void {
        $channel->send([new ChannelMessage('hello')]);
    }, [$channel]);

    $runtime->close();
    unset($channel);
    echo "OK\n";
}
?>
--EXPECT--
OK
OK
