--TEST--
parallel Future done
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();
$sync  = \parallel\Channel::make("sync");

$future = $parallel->run(function(){
    $sync = \parallel\Channel::open("sync");
    
    $sync->recv();
      
	return [42];
});

var_dump($future->done());

$sync->send(true);

$parallel->close();

var_dump($future->done());
?>
--EXPECT--
bool(false)
bool(true)

