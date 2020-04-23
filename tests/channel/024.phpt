--TEST--
Check channel closed check
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
  echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

$channel = Channel::make("io");
var_dump($channel->closed());
$channel->close();
var_dump($channel->closed());

?>
--EXPECT--
bool(false)
bool(true)



