--TEST--
Check Channel closures inside object properties
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Runtime, Channel};

$runtime = new Runtime;
$channel = Channel::make("channel");

$runtime->run(function($channel){
    $data = $channel->recv();

    ($data->closure)();
}, [$channel]);

$std = new stdClass;
$std->closure = function(){
    echo "OK";
};

$channel->send($std);
?>
--EXPECT--
OK

