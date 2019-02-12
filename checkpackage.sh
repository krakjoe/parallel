#!/bin/sh

for i in $(find tests -type f) $(find src -name \*.c -o -name \*.h)
do
  k=$(basename $i)
  grep -q $k package.xml || echo missing $k
done
