#!/bin/bash
# render-match20: the real-zoom-1.5 MBTL captures, one game at a time through run.sh's lock.
# Retries while another agent's game is still running (run.sh refuses to start then).
D=/mnt/c/dev/hantei-chan/tools/gamecap
for c in ${CASES:-v2r20_mbtl_chr001_zoom15}; do
  for try in $(seq 1 60); do
    "$D/run.sh" "$D/cases/$c.gcs" > "$D/round20/run_$c.log" 2>&1; rc=$?
    grep -q "a game is already running" "$D/round20/run_$c.log" || break
    sleep 20
  done
  echo "$c rc=$rc"
done
