--TEST--
Check Events polling fallback with more than 1000 Channels
--DESCRIPTION--
Registers 1100 idle Channels before a delayed Future. On typical POSIX systems
this exceeds select's FD_SETSIZE while collecting notification descriptors, so
Events must fall back to busy polling and still deliver the Future before the
timeout. Other platforms still exercise delivery through a large target set.
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
use parallel\Channel;
use parallel\Events;
use parallel\Runtime;

$start = new Channel();
$runtime = new Runtime();
$future = $runtime->run(static function (Channel $start): int {
    $start->recv();
    usleep(20000);

    return 42;
}, [$start]);
$events = new Events();

for ($i = 0; $i < 1100; $i++) {
    $events->addChannel(new Channel());
}

$events->addFuture('future', $future);
$events->setTimeout(1000000);
$start->send(true);
$event = $events->poll();

if ($event->source !== 'future' || $event->value !== 42) {
    echo "FAIL fallback\n";
    return;
}

echo "OK\n";
?>
--EXPECT--
OK
