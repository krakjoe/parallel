--TEST--
Check buffered Channel drains
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$chan = Channel::make("hi", 10001);
$limit = 10;

for($i = 0; $i <= $limit; ++$i){
    $chan->send($i);
}

$chan->close();

while (($value = $chan->recv()) > -1) {
    var_dump($value);

    if ($value == $limit)
        break;
}

try {
    $chan->recv();
} catch (\parallel\Channel\Error\Closed $ex) {
    var_dump($ex->getMessage());
}
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
string(18) "channel(hi) closed"


