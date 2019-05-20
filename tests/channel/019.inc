<?php
class Foo {
    private $int = 1;

    private $closure;

    public function __construct(Closure $closure) {
        $this->closure = $closure;
    }

    public function call() {
        return ($this->closure)();
    }
}
