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
    echo "OK";
}
?>
--EXPECT--
BLOCKER
BLOCKER
OK


