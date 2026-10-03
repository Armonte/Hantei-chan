#!/bin/bash
# continue -> new fight -> combo, N times
W=/mnt/c/dev/hantei-chan/work
PID=$(cat $W/last_gof_pid.txt|tr -d '\r\n ')
for c in $(seq 1 $1); do
  cp $W/live/seq_cont.txt $W/live/inject.txt
  sleep 8
  timeout 60 python.exe 'C:\dev\hantei-chan\work\live\wait_battle.py' $PID 55 >/dev/null
  sleep 3
  cp $W/live/seq_combo.txt $W/live/inject.txt
  sleep 35
done
