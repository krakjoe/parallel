--TEST--
Check events loop basic
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;
use \parallel\Events;
use \parallel\Events\Event;
use \parallel\Events\Input;

$channel = Channel::make("buffer", Channel::Infinite);

$input = new Input();
$input->add("buffer", "input");

$events = new Events($input);
$events->addChannel($channel);

foreach ($events as $event) {
    switch ($event->type) {
        case Event::Read:
            var_dump($event->value);
            return;
        
        case Event::Write:
            $events->addChannel($channel);
        break;
    }
}
?>
--EXPECT--
string(5) "input"




