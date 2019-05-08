--TEST--
Check closures statics
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Channel, Runtime};

$runtime = new Runtime();

$channel = Channel::make("channel", Channel::Infinite);

$runtime->run(function(){
    $channel = Channel::open("channel");

    while (($closure = $channel->recv())) {
        $closure();
    }
});

for ($i = 0; $i<10; $i++)
$channel->send(function(){
    static $vars = [452];

    var_dump($vars);
});

$channel->send(false);
?>
--EXPECT--
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}
array(1) {
  [0]=>
  int(452)
}

