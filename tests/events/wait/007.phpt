--TEST--
Check Events blocker
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Events;
use \parallel\Channel;

$events = new Events();
$events->addChannel(Channel::make("buffer"));
$events->setBlocker(function(){
    static $calls = 0;

    echo
        "BLOCKER\n";
    /* interrupt loop */
    return ++$calls == 2;
});

if ($events->poll() === null && count($events)) {
    echo "OK\n";
}

$events = new Events();
$events->addChannel(Channel::make("timeout"));
$events->setTimeout(1);
$events->setBlocker(fn() => false);

try {
    $events->poll();
} catch (Events\Error\Timeout $error) {
    echo "TIMEOUT";
}
?>
--EXPECT--
BLOCKER
BLOCKER
OK
TIMEOUT


