--TEST--
persistent copy of object with user CE must not segfault after thread exit
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	echo 'skip';
}
?>
--FILE--
<?php
/*
 * When a thread throws an exception whose trace contains a plain user object
 * (a class without create_object), the exception is persistently copied.
 * The persistent copy must not keep the thread-local zend_class_entry pointer,
 * because it becomes dangling after the thread exits.
 *
 * Without the fix, php_parallel_copy_object_dtor() calls
 * instanceof_function() on the dangling CE and segfaults during shutdown.
 *
 * We use Events with a Future to mirror the amphp pattern that triggers the
 * crash: during shutdown, Events destroys the Future which calls
 * php_parallel_exceptions_destroy() on the persistent exception data.
 */
$runtime = new \parallel\Runtime(sprintf("%s/073-bootstrap.inc", __DIR__));

$future = $runtime->run(function(){
    $obj = new PlainUserClass("test");
    throw_with_object_in_trace($obj);
});

/* Wait for the thread to finish */
try {
    $future->value();
} catch (\Throwable $e) {
    echo $e->getMessage() . "\n";
}

/*
 * Kill the runtime to ensure the thread is fully shut down and its
 * class entries are freed. The $future still holds persistently copied
 * exception data referencing the (now-freed) thread-local CE.
 */
$runtime->kill();

/*
 * Add the future to Events so its cleanup happens via Events::remove()
 * during shutdown — this is the path that triggers the segfault in the
 * original amphp use case.
 */
$events = new \parallel\Events();
$events->addFuture("task", $future);

/* Force some allocations to increase chance of CE memory being reused */
for ($i = 0; $i < 1000; $i++) {
    ${"v$i"} = str_repeat("x", 256);
}

echo "OK\n";
/* Segfault would occur here during shutdown when Events destroys the Future */
?>
--EXPECT--
error from thread
OK
