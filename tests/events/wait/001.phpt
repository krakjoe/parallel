--TEST--
Check events wait basic
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
use \parallel\Events\Payloads;

$channel = Channel::make("buffer", Channel::Infinite);

$events = new Events();
$events->addTargetChannel($channel);

$payloads = new Payloads();
$payloads->add("buffer", "input");

while (($result = $events->wait($payloads))) {
    switch ($result->type) {
        case Event::Read:
            var_dump($result->value);
            return;
        
        case Event::Write:
            $events->addTargetChannel($channel);
        break;
    }
}

?>
--EXPECT--
string(5) "input"




