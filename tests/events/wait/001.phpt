--TEST--
Check events loop channel
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

$events = new Events();
$events->addChannel($channel);

$input = new Input();
$input->add("buffer", "input");

$events->setInput($input);

foreach ($events as $event) {
    switch ($event->type) {
        case Event\Type::Read:
            var_dump($event->value);
            return;
        
        case Event\Type::Write:
            $events->addChannel($channel);
        break;
    }
}
?>
--EXPECT--
string(5) "input"




