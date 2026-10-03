#!/bin/bash
# wait for title then run the sequence given as args
cd /mnt/c/dev/hantei-chan/work
python3 waittitle.py 300 >/dev/null
live/press.sh "$@"
