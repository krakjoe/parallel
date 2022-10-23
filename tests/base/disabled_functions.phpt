--TEST--
parallel disabled functions
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
if (!function_exists("dl") && !function_exists("setlocale")) {
    die("skip nothing to check with");
}
?>
--FILE--
<?php
$parallel = new \parallel\Runtime();

if (function_exists("setlocale")) {
    $future = $parallel->run(function() {
        try {
            setlocale(LC_ALL, "0");
        } catch (Error $e) {
            if ($e->getMessage() !== 'Call to undefined function setlocale()') {
                echo "failed check function setlocale disabling" . PHP_EOL;
            }
        }
    });

    $future->value();

    setlocale(LC_ALL, "0");
}

if (function_exists("dl")) {
    $future = $parallel->run(function() {
        try {
            dl(":/invalid");
        } catch (Error $e) {
            if ($e->getMessage() !== 'Call to undefined function dl()') {
                echo "failed check function dl disabling" . PHP_EOL;
            }
        }
    });
    $future->value();
}

echo "Done" . PHP_EOL;

?>
--EXPECT--
Done

