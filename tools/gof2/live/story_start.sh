#!/bin/bash
cd /mnt/c/dev/hantei-chan/work
python3 waittitle.py 300 >/dev/null
S=("0100 0000 150" "0000 0000 1800" "0100 0000 150" "0000 0000 2500" "0100 0000 150" "0000 0000 1500")
for i in $(seq 1 14); do S+=("0100 0000 120" "0000 0000 800"); done
live/press.sh "${S[@]}"
