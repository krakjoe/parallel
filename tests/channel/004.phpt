--TEST--
Check channel operation (buffered)
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel  = Channel::make("io", true);

$parallel = new parallel\Runtime();

$parallel->run(function($channel){
	$channel = Channel::open($channel);

	for ($count = 0; $count <= 10; $count++) {
	    $channel->send($count);
	}
	
	$channel->send(false);
}, [(string) $channel]);

while (($value = $channel->recv()) !== false) {
    var_dump($value);
}
?>
--EXPECT--
int(0)
int(1)
int(2)
int(3)
int(4)
int(5)
int(6)
int(7)
int(8)
int(9)
int(10)

