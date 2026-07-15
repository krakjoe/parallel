--TEST--
Check Events channel readiness levels
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

$channel = Channel::make('notify-level', 1);
$channel->send(1);
$events = new Events();
$events->addChannel($channel);

if ($events->poll()->value !== 1) {
    echo "FAIL preloaded\n";
    return;
}

$events->addChannel($channel);
$events->setTimeout(1000);

try {
    $events->poll();
    echo "FAIL lowered\n";
    return;
} catch (Events\Error\Timeout $error) {
}

$runtime = new Runtime();
$future = $runtime->run(static function (Channel $channel): void {
    usleep(20000);
    $channel->send(2);
}, [$channel]);
$events = new Events();
$events->addChannel($channel);

if ($events->poll()->value !== 2) {
    echo "FAIL reraised\n";
    return;
}

$future->value();

$channel = Channel::make('notify-close');
$read = new Events();
$read->addChannel($channel);
$read->setTimeout(1000);

try {
    $read->poll();
} catch (Events\Error\Timeout $error) {
}

$input = new Events\Input();
$input->add('notify-close', true);
$write = new Events();
$write->setInput($input);
$write->addChannel($channel);
$write->setTimeout(1000);

try {
    $write->poll();
} catch (Events\Error\Timeout $error) {
}

$channel->close();

if ($read->poll()->type !== Type::Close || $write->poll()->type !== Type::Close) {
    echo "FAIL close\n";
    return;
}

echo "OK\n";
?>
--EXPECT--
OK
