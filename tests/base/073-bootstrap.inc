<?php
class PlainUserClass {
    public string $name;

    public function __construct(string $name) {
        $this->name = $name;
    }
}

function throw_with_object_in_trace(PlainUserClass $obj) {
    throw new RuntimeException("error from thread");
}
