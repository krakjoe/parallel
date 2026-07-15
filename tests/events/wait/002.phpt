--TEST--
Check events loop future
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
use \parallel\Events\Input;

$parallel = new \parallel\Runtime();

$channel = Channel::make("channel", Channel::Infinite);

$input = new Input();
$input->add("channel", "input");

$events = new Events();
$events->setInput($input);
$events->setTimeout(1000000);
$events->addChannel($channel);
$events->addFuture("future", $parallel->run(function(){
    return [42];
}));

$seen = [];

try {
    foreach ($events as $event) {
        switch ($event->type) {
            case Event\Type::Read:
                if ($event->object instanceof Future &&
                    $event->value == [42]) {
                    $seen['future'] = true;
                    echo "OK\n";
                }

                if ($event->object instanceof Channel &&
                    $event->value == "input") {
                    $seen['channel'] = true;
                    echo "OK\n";
                }
            break;

            case Event\Type::Write:
                $events->addChannel($channel);
            break;
        }
    }
} catch (Events\Error\Timeout $error) {
    printf("TIMEOUT future=%d channel=%d targets=%d\n",
        isset($seen['future']), isset($seen['channel']), count($events));
}
?>
--EXPECT--
OK
OK





