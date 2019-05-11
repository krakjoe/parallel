<?php
/**
* @doc
**/
return function(stdClass $std, ...$varargs) : stdClass {
    try {
        for ($i = 0; $i < 100; $i++);

    } catch (Throwable $t) {

    } finally {
        return $std;
    }
}
?>
