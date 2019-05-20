--TEST--
Check closures static changes
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

$future = $runtime->run(function(){
    $channel = Channel::open("channel");
    
    while ($closure = $channel->recv()) {
        $closure(false, false, true);
    }
});

$closure = function(bool $three, bool $four, bool $dump = false) {
    static $vars = [
        1 => "one",
        2 => "two"
    ];

    if ($three)
        $vars[3] = "three";

    if ($four)
        $vars[4] = "four";
    
    if ($dump) {
        var_dump($vars);
    }
};

$channel->send($closure);

$closure(true, false);
$channel->send($closure);

$closure(true, true);
$channel->send($closure);

$channel->send(false);
?>
--EXPECT--
array(2) {
  [1]=>
  string(3) "one"
  [2]=>
  string(3) "two"
}
array(3) {
  [1]=>
  string(3) "one"
  [2]=>
  string(3) "two"
  [3]=>
  string(5) "three"
}
array(4) {
  [1]=>
  string(3) "one"
  [2]=>
  string(3) "two"
  [3]=>
  string(5) "three"
  [4]=>
  string(4) "four"
}

