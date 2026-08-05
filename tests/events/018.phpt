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

$channel = Channel::make('notify-buffered-write', 1);
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
    $input->add('notify-buffered-write', $i);
    $events->addChannel($channel);

    if ($events->poll()->type !== Type::Write) {
        echo "FAIL buffered write\n";
        return;
    }
}

if ($future->value() !== 4949) {
    echo "FAIL sum\n";
    return;
}

$channel = Channel::make('notify-unbuffered-write');
$future = $runtime->run(static function (Channel $channel): int {
    usleep(20000);
    return $channel->recv();
}, [$channel]);
$input = new Events\Input();
$input->add('notify-unbuffered-write', 42);
$events = new Events();
$events->setInput($input);
$events->addChannel($channel);

if ($events->poll()->type !== Type::Write || $future->value() !== 42) {
    echo "FAIL unbuffered write\n";
    return;
}

$start = Channel::make('notify-future');
$future = $runtime->run(static function (Channel $start): int {
    $start->recv();
    usleep(20000);

    return 42;
}, [$start]);
$events = new Events();

for ($i = 0; $i < 64; $i++) {
    $events->addChannel(new Channel());
}

$events->addFuture('future', $future);
$start->send(true);
$event = $events->poll();

if ($event->source !== 'future' || $event->value !== 42) {
    echo "FAIL future\n";
    return;
}

$first = new Events();
$second = new Events();
$first->addFuture('first', $future);
$second->addFuture('second', $future);

if ($first->poll()->value !== 42 || $second->poll()->value !== 42) {
    echo "FAIL observers\n";
    return;
}

$future = $runtime->run(static function (): void {
    usleep(20000);
    throw new RuntimeException('failed');
});
$events = new Events();
$events->addFuture('error', $future);
$event = $events->poll();

if ($event->type !== Type::Error || !$event->value instanceof RuntimeException) {
    echo "FAIL error\n";
    return;
}

echo "OK\n";
?>
--EXPECT--
OK
