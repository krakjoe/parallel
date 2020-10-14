--TEST--
parallel cache unset flags on literals
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}

if (ini_get("opcache.enable_cli")) {
	die("skip opcache must not be loaded");
}
?>
--FILE--
<?php
use parallel\Channel;
use parallel\Runtime;
$sarr=[];

for ($i=0;$i<3;$i++){
	$sarr[$i]=new Runtime();
}

$ch = Channel::make("sv", 3);

for ($i=0;$i<3;$i++){
	$sarr[$i]->run(function() use($ch) {
		$functions = array("addslashes", "chunk_split", "metaphone", "strip_tags", "md5", "sha1", "strtoupper", "strtolower", "strrev", "strlen", "soundex", "ord");
		$string = "the quick brown fox jumps over the lazy dog";
		
		for ($i = 0; $i < 4000; $i++) {
			foreach ($functions as $function) {
				call_user_func_array($function, array($string));
			}
		}

		$ch->send(time());
	});

}

for ($i=0;$i<3;$i++){
	var_dump($ch->recv());
}

echo "ok";
?>
--EXPECTF--
int(%d)
int(%d)
int(%d)
ok
