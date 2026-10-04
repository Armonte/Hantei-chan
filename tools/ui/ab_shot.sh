#!/bin/bash
# tools/ui/ab_shot.sh <script> : run the built editor on a --ui-script file (see src/startup_args.h) from a scratch folder on C:.
# No real mouse / keyboard is touched: every action comes from the in-app script runner; `capture <png>` reads the app's own GL frame.
D="$(cd "$(dirname "$0")/../.." && pwd)"; W=/mnt/c/dev/hantei-chan/work/ab
mkdir -p "$W"
cp "$D/build/gonptechan.exe" "$W/gonptechan.exe"
cp "$1" "$W/script.txt"
rm -f "$W/script.txt.log"
# fresh layout each run (AB_KEEP_INI=1 keeps it); a 1900x1000 window like a real desktop session
if [ -z "$AB_KEEP_INI" ]; then rm -f "$W/hanteichan.ini"; printf "[Other settings][]\nposX=10\nposY=10\nsizeX=${AB_W:-1900}\nsizeY=${AB_H:-1000}\n" > "$W/hanteichan.ini"; fi
cd "$W" && timeout "${AB_TIMEOUT:-120}" ./gonptechan.exe --ui-script 'C:\dev\hantei-chan\work\ab\script.txt'
echo "exit $?"; [ -f "$W/script.txt.log" ] && cat "$W/script.txt.log"
