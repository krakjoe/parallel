<?php
class Foo {

    public function getClosure() {
        return function() : Foo {
            return $this;
        };
    }
}
?>
