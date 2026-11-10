<?php
class Renamed {
    public $a;
    public $b;

    public function __construct($a, $b) {
        $this->a = $a;
        $this->b = $b;
    }
}

class Typed {
    public $a;

    public function __construct($a) {
        $this->a = $a;
    }
}
