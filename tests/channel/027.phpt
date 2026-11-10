--TEST--
Check Channel rejects objects with same property count but different layout
--DESCRIPTION--
A matching property count alone does not catch classes that are declared
with the same number of properties but different names or types: values
would be copied into the wrong properties, or typed properties would hold
values of the wrong type. The layout fingerprint must reject these too.
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
use parallel\{Channel, Runtime};

/* same property count, different property names */
class Renamed {
    public $x;
    public $y;
}

/* same property name, different property type */
class Typed {
    public int $a;
}

$runtime = new Runtime(__DIR__ . '/027.inc');

$renamed = new Channel(1);
$typed = new Channel(1);

$runtime->run(static function (Channel $renamed): void {
    $renamed->send(new Renamed('first', 'second'));
}, [$renamed]);

$runtime->run(static function (Channel $typed): void {
    $typed->send(new Typed('7'));
}, [$typed]);

$runtime->close();

foreach ([$renamed, $typed] as $channel) {
    try {
        $channel->recv();
    } catch (\parallel\Channel\Error\IllegalValue $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
parallel\Channel\Error\IllegalValue: cannot receive object of class Renamed, sending and receiving runtimes declare it differently
parallel\Channel\Error\IllegalValue: cannot receive object of class Typed, sending and receiving runtimes declare it differently
