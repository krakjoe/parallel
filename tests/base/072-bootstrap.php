<?php
function transformChunk($n)
{
  $fibonacci = function ($n) use (&$fibonacci) {
    if ($n == 0) {
      return 0;
    }
    if ($n == 1) {
      return 1;
    }
    return $fibonacci($n - 1) + $fibonacci($n - 2);
  };
  return $fibonacci($n);
}
