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
    echo 
        "BLOCKER\n";
    /* interrupt loop */
    return true;
});

if ($events->poll() === null && count($events)) {
    echo "OK";
}
?>
--EXPECT--
BLOCKER
OK


