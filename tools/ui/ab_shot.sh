#!/bin/bash
# tools/ui/ab_shot.sh <script> : run the built editor on a --ui-script file (see src/startup_args.h) from a scratch folder on C:.
# No real mouse / keyboard is touched: every action comes from the in-app script runner; `capture <png>` reads the app's own GL frame.
D="$(cd "$(dirname "$0")/../.." && pwd)"; W=/mnt/c/dev/hantei-chan/work/ab
mkdir -p "$W"
cp "$D/build/gonptechan.exe" "$W/gonptechan.exe"
cp "$1" "$W/script.txt"
rm -f "$W/script.txt.log"
cd "$W" && timeout "${AB_TIMEOUT:-120}" ./gonptechan.exe --ui-script 'C:\dev\hantei-chan\work\ab\script.txt'
echo "exit $?"; [ -f "$W/script.txt.log" ] && cat "$W/script.txt.log"
