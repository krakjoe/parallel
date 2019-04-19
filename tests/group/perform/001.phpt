--TEST--
Check group perform basic
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;
use \parallel\Group;
use \parallel\Group\Result;
use \parallel\Group\Payloads;

$channel = Channel::make("buffer", Channel::Infinite);

$group = new Group();
$group->add($channel);

$payloads = new Payloads();
$payloads->add("buffer", "input");

while (($result = $group->perform($payloads))) {
    switch ($result->type) {
        case Result::Read:
            var_dump($result->value);
            return;
        
        case Result::Write:
            $group->add($channel);
        break;
    }
}

?>
--EXPECT--
string(5) "input"




