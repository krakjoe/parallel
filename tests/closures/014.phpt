--TEST--
Check closures over internal functions are rejected
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
use \parallel\{Runtime, Channel};

$runtime = new Runtime();
$internal = Closure::fromCallable('strtoupper');

try {
	$runtime->run(function($closure){}, [$internal]);
} catch (Error $ex) {
	printf("%s: %s\n", get_class($ex), $ex->getMessage());
}

try {
	$runtime->run(function($array){}, [[$internal]]);
} catch (Error $ex) {
	printf("%s: %s\n", get_class($ex), $ex->getMessage());
}

try {
	Channel::make("channel", 1)->send($internal);
} catch (Error $ex) {
	printf("%s: %s\n", get_class($ex), $ex->getMessage());
}

$future = $runtime->run(function(){
	return Closure::fromCallable('strtoupper');
});

var_dump($future->value());
?>
--EXPECTF--
parallel\Runtime\Error\IllegalParameter: illegal parameter (Closure) passed to task at argument 1
parallel\Runtime\Error\IllegalParameter: illegal parameter (Closure) passed to task at argument 1
parallel\Channel\Error\IllegalValue: value of type Closure is illegal
object(parallel\Runtime\Object\Unavailable)#%d (0) {
}
