--TEST--
parallel Runtime continues after task exit with included class and namespace imports
--SKIPIF--
<?php
if (!extension_loaded('parallel')) {
	die("skip parallel not loaded");
}
if (PHP_VERSION_ID < 80400) {
    die("skip php 8.4 required");
}
?>
--FILE--
<?php
declare(strict_types=1);

use parallel\Runtime;

error_reporting(E_ALL);

class Flow
{
    protected(set) Runtime $runtime;
    protected array $futures = [];

    public function __construct(private string $log)
    {
        $this->runtime = new Runtime;
    }

    public function exec(Closure $callback): self
    {
        $wrapTask = static function (Closure $task, string $log): void {
            file_put_contents($log, "task exit\n", FILE_APPEND);
            exit;
        };

        $this->futures[] = $this->runtime->run($wrapTask, [$callback, $this->log]);

        return $this;
    }

    public function wait(): void
    {
        foreach ($this->futures as $future) {
            try {
                $future->value();
            } catch (Throwable) {
            }
        }
    }
}

$log = tempnam(sys_get_temp_dir(), 'parallel-074-');
$flow = new Flow($log);

$boot = $flow->runtime->run(static function (): void {
    require_once __DIR__ . '/074-autoload.inc';
    require_once __DIR__ . '/074-fn.inc';
    echo "boot\n";
});
$boot->value();

$flow->exec(static function (): void {});
$flow->exec(static function (): void {});
$flow->exec(static function (): void {});

$flow->wait();

echo "task exits=" . substr_count(file_get_contents($log), "task exit\n") . "\n";
@unlink($log);

exit("main flow end\n");
?>
--EXPECT--
boot
task exits=3
main flow end
