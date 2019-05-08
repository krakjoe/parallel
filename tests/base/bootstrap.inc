<?php
function bootstrapped() {
	return true;
}

function thrower() {
    throw new Exception();
}

class Qux {
    public function method($array, $object) {
        throw new RuntimeException("message", 42);
    }
}

class Foo {
    public function bar($array, $object) {
        return (new Qux())
            ->method($array, $object);
    }
}
