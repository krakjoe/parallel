--TEST--
parallel streams
--SKIPIF--
<?php
if (!extension_loaded('parallel') || strtoupper(substr(PHP_OS, 0, 3)) == 'WIN') {
	die("skip non windows test");
}
?>
--FILE--
<?php
$serving = new \parallel\Runtime();
$sending = new \parallel\Runtime();

$server = stream_socket_server("tcp://127.0.0.1:9999", $errno, $errstr);

$serving->run(function($server) {
	$server = fopen("php://fd/{$server}", "r+");

	if ($client = stream_socket_accept($server, 10)) {
		echo "OK\n";
	}
}, [$server]);

$sending->run(function(){
	$sock = fsockopen("127.0.0.1", 9999);
	
	if ($sock) {
		fclose($sock);
	}
});
?>
--EXPECT--
OK
