#!/bin/bash
# tools/ui/ab_shot.sh <script> : run the built editor on a --ui-script file (see src/startup_args.h) from a scratch folder on C:.
# No real mouse / keyboard is touched: every action comes from the in-app script runner; `capture <png>` reads the app's own GL frame.
# Hidden window + offscreen render by default (--headless); AB_VISIBLE=1 shows the window as before.
HL=(--headless); [ -n "${AB_VISIBLE:-}" ] && HL=()
D="$(cd "$(dirname "$0")/../.." && pwd)"
# One scratch folder per worktree (AB_WORK overrides): parallel streams used to overwrite each other's exe and script in work/ab.
W="${AB_WORK:-/mnt/c/dev/hantei-chan/work/ab_$(basename "$D")}"
mkdir -p "$W"
cp "${AB_EXE:-$D/build/gonptechan.exe}" "$W/gonptechan.exe"   # AB_EXE: run another build (e.g. a before/after pair)
# shot scripts name their captures under C:/dev/hantei-chan/work/ab/; point them at this worktree's folder
WW="$(wslpath -m "$W")"
sed "s#C:/dev/hantei-chan/work/ab/#$WW/#g" "$1" > "$W/script.txt"
rm -f "$W/script.txt.log"
# fresh layout each run (AB_KEEP_INI=1 keeps it); a 1900x1000 window like a real desktop session
if [ -z "$AB_KEEP_INI" ]; then rm -f "$W/hanteichan.ini"; printf "[Other settings][]\nposX=10\nposY=10\nsizeX=${AB_W:-1900}\nsizeY=${AB_H:-1000}\n" > "$W/hanteichan.ini"; fi
cd "$W" && timeout "${AB_TIMEOUT:-120}" ./gonptechan.exe "${HL[@]}" --ui-script "$(wslpath -w "$W/script.txt")"
echo "exit $?"; [ -f "$W/script.txt.log" ] && cat "$W/script.txt.log"
