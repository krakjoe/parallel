<?php

echo "bootstrap\n";
register_shutdown_function(static function () {
    echo "shutdown\n";
});
