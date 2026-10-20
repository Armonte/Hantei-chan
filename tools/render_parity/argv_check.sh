#!/bin/bash
# tools/render_parity/argv_check.sh [exe-dir] [out-dir]
#
# Command-line path arguments reach the loaders in the ANSI code page, as the
# pre-SDL wWinMain gave them (WideCharToMultiByte(CP_ACP)); SDL's own argv is
# UTF-8. Copies MBAACC akiha into a folder named "café" (é is the one byte 0xE9
# in code page 1252, two bytes C3 A9 in UTF-8) and runs the exe headless with
# --open on it. Pass: the "startup: --open" log line holds the code page 1252
# byte 0xE9 in "caf\xE9\akiha.txt" (quill escapes it as that text), not the UTF-8
# pair C3 A9.
#
# What happens next is the baseline's behaviour, reported, not judged: the
# loaders read akiha.txt and its four HA6 files through the ANSI file APIs,
# then CharacterInstance::loadFromTxt's std::filesystem::path(narrow string)
# throws "Cannot convert character sequence" (libstdc++ converts with the C
# locale) and the process ends with exit 3. The golden binary does exactly the
# same with the same command (render_d3d9/STATUS.md, P0 fix 3).
# Exit 0 = pass. Outputs in <out-dir> (default
# /mnt/c/dev/hantei-chan/work/argv_check).
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
X="${1:-$D/build}"
W="${2:-/mnt/c/dev/hantei-chan/work/argv_check}"
mkdir -p "$W/café" "$W/run"
for f in /mnt/c/games/mbaacc/data/akiha.txt /mnt/c/games/mbaacc/data/akiha.HA6 /mnt/c/games/mbaacc/data/akiha_[0-9].HA6 \
	/mnt/c/games/mbaacc/data/akiha.cg /mnt/c/games/mbaacc/data/akiha.pal; do
	[ -f "$f" ] && cp "$f" "$W/café/"
done
cp "$X/gonptechan.exe" "$W/run/" && cp -r "$X/res" "$W/run/" 2>/dev/null
rm -f "$W/stats.txt" "$W/run/hanteichan.ini" "$W/run/hanteichan.log"
printf 'wait 40\nquit\n' > "$W/run/script.txt"
WW="$(wslpath -w "$W")"
(cd "$W/run" && timeout 90 ./gonptechan.exe --headless --deterministic --open "$WW\\café\\akiha.txt" \
	--stats "$WW\\stats.txt" --ui-script "$(wslpath -w "$W/run/script.txt")")
echo "exit $?"
LINE="$(grep -a 'startup: --open ' "$W/run/hanteichan.log" | tail -1)"
echo "log: $LINE"
# quill writes a non-ASCII byte as the text \xNN: code page 1252 gives caf\xE9,
# UTF-8 would give caf\xC3\xA9 (a raw byte is accepted too).
if printf '%s' "$LINE" | grep -aqF 'caf\xE9\akiha.txt' || printf '%s' "$LINE" | LC_ALL=C grep -aq $'caf\xe9\\\\akiha.txt'; then
	echo "argv_check: PASS (the --open value is in code page 1252)"; exit 0
fi
echo "argv_check: FAIL"; exit 1
