--TEST--
persistent copy of object with user CE must not segfault after thread exit
--DESCRIPTION--
When a thread throws an exception whose trace contains a plain user object
(a class without `create_object`), the exception is persistently copied.
The persistent copy must not keep the thread-local `zend_class_entry` pointer,
because it becomes dangling after the thread exits. Without the fix,
`php_parallel_copy_object_dtor()` calls `instanceof_function()` on the
dangling CE and segfaults during shutdown.
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
    echo 'skip';
}
?>
--FILE--
<?php
$runtime = new \parallel\Runtime(sprintf("%s/073-bootstrap.inc", __DIR__));

$future = $runtime->run(function () {
    $obj = new PlainUserClass("test");
    throw_with_object_in_trace($obj);
});

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

/* Segfault would occur here */
unset($future);

echo "OK\n";
?>
--EXPECT--
error from thread
OK
