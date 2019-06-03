--TEST--
Check anonymous Channel
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\Channel;

var_dump(new Channel);
var_dump(new Channel(Channel::Infinite));
var_dump(new Channel(1));

$create = function() {
    return new Channel;
};

var_dump((string) $create(), (string)$create());

function create() {
    return new Channel;
}

var_dump((string) create(), (string) create());

class Create {
    public static function channel() {
        return new Channel;
    }
}

var_dump((string) Create::channel(), (string) Create::channel());
?>
--EXPECTF--
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php#%d@%s[1]"
  ["type"]=>
  string(10) "unbuffered"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php#%d@%s[2]"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  string(8) "infinite"
}
object(parallel\Channel)#%d (%d) {
  ["name"]=>
  string(%d) "%s021.php#%d@%s[3]"
  ["type"]=>
  string(8) "buffered"
  ["capacity"]=>
  int(1)
}
string(%d) "%s021.php#9@%s[4]"
string(%d) "%s021.php#9@%s[5]"
string(%d) "create#15@%s[6]"
string(%d) "create#15@%s[7]"
string(%d) "Create::channel#22@%s[8]"
string(%d) "Create::channel#22@%s[9]"

