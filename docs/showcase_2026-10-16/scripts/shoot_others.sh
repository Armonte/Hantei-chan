#!/bin/bash
# shoot_others.sh <sosfiro|gonp> <case>...: render s1_compare cases with sosfiro's fork or gonp's upstream Hantei-chan,
# on a black / white / 64-grey clear colour (<case>_new_{k,w,g}.png, the same names and view as shoot_batch.sh gives our
# editor), so v2_model.py treats every tool the same way. Window-only GL back-buffer readback; no desktop capture, no input.
#   sosfiro: /mnt/c/dev/Hantei-chan/work/sosfiro_capture/build/hanteichan.exe (capture_args.cpp, render code untouched)
#   gonp:    gonpgonp/Hantei-chan master 7e97e19 + scripts/gonp_capture_args.patch (built in /home/teo/dev/hantei-chan-wt/gonp_upstream)
# Writes <case>_status.txt (gonp: load/pattern result; sosfiro: "ok"/"no shot") next to the shots.
set -u
TOOL=$1; shift
P=/home/teo/dev/hantei-chan-wt/parity; S=$P/docs/showcase_2026-10-16
CASES_FILES="$P/tools/uni/s1_compare/cases.txt $P/tools/uni/s1_compare/cases_control.txt $S/scripts/cases_mbaacc.txt"
OUT=$S/work/bw_$TOOL; W=/mnt/c/dev/hantei-chan/work/showcase_$TOOL${WSUF:-}; mkdir -p "$OUT" "$W"
case $TOOL in
  sosfiro) EXE=/mnt/c/dev/Hantei-chan/work/sosfiro_capture/build/hanteichan.exe; cp /mnt/c/dev/Hantei-chan/work/sosfiro_capture/build/libiconv.dll "$W/" 2>/dev/null || cp /mnt/c/dev/Hantei-chan/work/sosfiro_capture/ogbuild/libiconv.dll "$W/";;
  gonp) EXE=/home/teo/dev/hantei-chan-wt/gonp_upstream/build/hanteichan.exe;;
  *) echo "tool?"; exit 2;;
esac
cp "$EXE" "$W/hanteichan.exe"
G='C:\dev\hantei-chan\gamedata'; WW="$(wslpath -w "$W")"
for want in "$@"; do
  line=$(cat $CASES_FILES | grep -v "^#" | awk -F'|' -v n="$want" '{k=$1; gsub(/ /,"",k)} k==n' | head -1)
  [ -z "$line" ] && { echo "no case: $want"; continue; }
  IFS='|' read -r name txt pat fr tick zoom note <<< "$line"
  name=$(echo $name); txt=$(echo $txt | tr / '\\'); pat=$(echo $pat); fr=$(echo $fr); tick=$(echo $tick); zoom=$(echo $zoom)
  st=""
  for bg in k w g; do
    c=$(case $bg in k) echo 0,0,0;; w) echo 255,255,255;; g) echo 64,64,64;; esac)
    rm -f "$W/hanteichan.ini" "$W/shot.png" "$W/status.txt"
    printf "[Other settings][]\nposX=40\nposY=40\n" > /dev/null
    args=(--open "$G\\$txt" --pattern "$pat" --frame "$fr" --zoom "$zoom" --view 1264 861 --bg "$c" --hide-ui --hide-boxes)
    if [ "$TOOL" = sosfiro ]; then args+=(--no-lines); [ "$tick" != "-1" ] && args+=(--tick "$tick"); fi
    if [ "$TOOL" = gonp ]; then args+=(--status "$WW\\status.txt"); fi
    (cd "$W" && timeout 180 ./hanteichan.exe "${args[@]}" --capture "$WW\\shot.png" </dev/null >/dev/null 2>&1)
    if [ -f "$W/shot.png" ]; then cp "$W/shot.png" "$OUT/${name}_new_$bg.png"; fi
    [ -f "$W/status.txt" ] && st=$(cat "$W/status.txt") || { [ -f "$W/shot.png" ] && st="ok" || st="no shot (process exited without writing a capture)"; }
  done
  [ "$TOOL" = gonp ] && [ "$tick" != "-1" ] && st="$st; NOTE gonp has no animation playback/AFHK, shows frame $fr static (case asks tick $tick)"
  echo "$st" > "$OUT/${name}_status.txt"; echo "$TOOL $name: $st"
done
