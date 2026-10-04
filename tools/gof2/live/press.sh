#!/bin/bash
# press.sh "m0 m1 ms" ...  -> writes inject sequence file
f=/mnt/c/dev/hantei-chan/work/live/inject.txt
: > $f
for s in "$@"; do echo "$s" >> $f; done
echo "0000 0000 0" >> $f
