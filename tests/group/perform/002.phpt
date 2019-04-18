--TEST--
Check group perform future
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Runtime;
use \parallel\Channel;
use \parallel\Future;
use \parallel\Group;

$parallel = new \parallel\Runtime();

$channel = Channel::make("channel", Channel::Infinite);

$group = new Group();

$group->add($channel);
$group->add("future", $parallel->run(function(){
    return [42];
}));

$payloads = [
    "channel" => "input"
];

while (($result = $group->perform($payloads))) {
    switch ($result->type) {
        case Group\Result::Read:
            if ($result->object instanceof Future &&
                $result->value == [42]) {
                echo "OK\n";
            }
            
            if ($result->object instanceof Channel &&
                $result->value == "input") {
                echo "OK\n";    
            }
        break;
        
        case Group\Result::Write:
            $group->add($channel);
        break;
    }
}
?>
--EXPECT--
OK
OK





