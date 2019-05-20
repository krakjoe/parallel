<?php
class Foo {

    public function getClosure() {
        return static function() : string {
            return self::class;
        };
    }
}
?>
