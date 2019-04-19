--TEST--
Check events wait future
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Runtime;
use \parallel\Channel;
use \parallel\Future;
use \parallel\Events;
use \parallel\Events\Event;
use \parallel\Events\Payloads;

$parallel = new \parallel\Runtime();

$channel = Channel::make("channel", Channel::Infinite);

$events = new Events();

$events->addTargetChannel($channel);
$events->addTargetFuture("future", $parallel->run(function(){
    return [42];
}));

$payloads = new Payloads();
$payloads->add("channel", "input");

while (($event = $events->wait($payloads))) {
    switch ($event->type) {
        case Event::Read:
            if ($event->object instanceof Future &&
                $event->value == [42]) {
                echo "OK\n";
            }
            
            if ($event->object instanceof Channel &&
                $event->value == "input") {
                echo "OK\n";    
            }
        break;
        
        case Event::Write:
            $events->addTargetChannel($channel);
        break;
    }
}
?>
--EXPECT--
OK
OK





