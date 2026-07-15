--TEST--
Check Events notification-driven wakeups
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
use parallel\Events\Event\Type;
use parallel\Runtime;

$runtime = new Runtime();
$channel = Channel::make('notify-read');
$future = $runtime->run(static function (Channel $channel): void {
    for ($i = 0; $i < 100; $i++) {
        $channel->send($i);
    }
}, [$channel]);
$events = new Events();

for ($i = 0; $i < 100; $i++) {
    $events->addChannel($channel);
    $event = $events->poll();

    if ($event->type !== Type::Read || $event->value !== $i) {
        echo "FAIL read\n";
        return;
    }
}

$future->value();

$channel = Channel::make('notify-write', 1);
$channel->send(-1);
$future = $runtime->run(static function (Channel $channel): int {
    $sum = $channel->recv();

    for ($i = 0; $i < 100; $i++) {
        $sum += $channel->recv();
    }

    return $sum;
}, [$channel]);
$input = new Events\Input();
$events = new Events();
$events->setInput($input);

for ($i = 0; $i < 100; $i++) {
    $input->add('notify-write', $i);
    $events->addChannel($channel);

    if ($events->poll()->type !== Type::Write) {
        echo "FAIL write\n";
        return;
    }
}

if ($future->value() !== 4949) {
    echo "FAIL sum\n";
    return;
}

$start = Channel::make('notify-future');
$future = $runtime->run(static function (Channel $start): int {
    $start->recv();
    usleep(20000);

    return 42;
}, [$start]);
$events = new Events();
$channels = [];

for ($i = 0; $i < 1000; $i++) {
    $channels[] = new Channel();
    $events->addChannel($channels[$i]);
}

$events->addFuture('future', $future);
$start->send(true);
$event = $events->poll();

if ($event->source !== 'future' || $event->value !== 42) {
    echo "FAIL future\n";
    return;
}

echo "OK\n";
?>
--EXPECT--
OK
