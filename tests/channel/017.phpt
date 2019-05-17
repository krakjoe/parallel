--TEST--
Check Channel closures inside arrays
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

    ($data["closure"])();
}, [$channel]);

$channel->send([
    "closure" => function(){
        echo "OK";
    }
]);
?>
--EXPECT--
OK

