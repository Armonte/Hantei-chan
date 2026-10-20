#!/bin/bash
# tools/render_parity/text_input_check.sh [exe] [out-dir]
#
# Visible type-to-search check (P0 fix 2, item 2). The old WndProc got WM_CHAR in
# every window whether or not an ImGui text field was active, so the archive
# browser's type-to-search (io.InputQueueCharacters) worked; under SDL a
# character only becomes SDL_EVENT_TEXT_INPUT while SDL text input is on for the
# window, which imgui_impl_sdl3 turns off whenever no field wants it. This runs
# the editor VISIBLY (AB_VISIBLE=1, through tools/ui/ab_shot.sh) on a folder of
# four characters (alpha, bravo, charlie, delta), cursor on alpha and ImGui focus
# on the browser window (script `focus`, what a click on it does: the OS focus of
# a window started from WSL comes and goes at startup, and ImGui's multi-viewport
# code then sets its window focus to none, which the type-to-search needs), twice:
#
#   control  nothing typed                              -> cursor 0 (alpha)
#   typed    "c" as a real SDL_EVENT_TEXT_INPUT pushed   -> cursor 2 (charlie)
#            at the main window (SDL_PushEvent), then
#            after 1.2 s (a new search) "d" as WM_CHAR   -> cursor 3 (delta), when the window has the keyboard
#            posted to the app's own main window (SDL's     focus (SDL drops text for an unfocused window, and a
#            window procedure makes it text input)          window started from WSL usually gets none; the run
#                                                           then reports it instead of judging it)
#
# and checks SDL text input is on for the main window with no field active.
# No desktop input: every event is the app's own; captures are in-app GL reads.
# Exit 0 = pass. Outputs stay in <out-dir> (default
# /mnt/c/dev/hantei-chan/render_parity/text_input_check).
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$D/build/gonptechan.exe}"
OUT="${2:-/mnt/c/dev/hantei-chan/render_parity/text_input_check}"
CHAR="$D/tests/fixtures/headless/chr000.ha6"
mkdir -p "$OUT/dir"
for n in alpha bravo charlie delta; do cp "$CHAR" "$OUT/dir/$n.ha6"; done
OUTW="$(wslpath -w "$OUT")"
OUTM="$(wslpath -m "$OUT")"
fail=0

run() {  # run <name> <script lines...>
	local name="$1"; shift
	rm -f "$OUT/${name}.png"
	{
		printf 'browse %s\\dir\nwaitidle\nwait 5\nkey Down\nwait 5\nfocus\ntextinput\n' "$OUTW"
		for l in "$@"; do printf '%s\n' "$l"; done
		printf 'wait 10\ninfo\ntextinput\ncapture %s/%s.png\nquit\n' "$OUTM" "$name"
	} > "$OUT/${name}_script.txt"
	AB_VISIBLE=1 AB_TIMEOUT=60 AB_EXE="$EXE" AB_WORK="/mnt/c/dev/hantei-chan/work/ab_text_input_check_$name" \
		bash "$D/tools/ui/ab_shot.sh" "$OUT/${name}_script.txt" > "$OUT/${name}_run.log" 2>&1
	echo "== $name: $(head -1 "$OUT/${name}_run.log")"
	sed 's/^/   /' "$OUT/${name}_run.log" | tail -n +2
}
cursors() { grep -o 'cursor=-\?[0-9]*' "$OUT/$1_run.log" | cut -d= -f2 | tr '\n' ' '; }
nth() { echo "$1" | awk -v n="$2" '{ print $n }'; }
# the keyboard focus when the WM_CHAR was posted: the textinput line logged right after it
wmfocus() { grep -A3 '^wmchar' "$OUT/$1_run.log" | grep -o 'keyboard_focus=[a-z]*' | head -1 | cut -d= -f2; }
common() {  # common <name>
	local name="$1"
	[ "$(head -1 "$OUT/${name}_run.log")" = "exit 0" ] || { echo "FAIL $name: $(head -1 "$OUT/${name}_run.log")"; fail=1; return 1; }
	[ -s "$OUT/${name}.png" ] || { echo "FAIL $name: no capture"; fail=1; }
	grep -q '^textinput main=1 want_text=0 ' "$OUT/${name}_run.log" \
		|| { echo "FAIL $name: SDL text input is off for the main window with no text field active"; fail=1; }
	return 0
}

run control
run typed "sdltext c" "wait 10" "info" "waitms 1200" "focus" "wmchar d" "textinput"
if common control; then
	c="$(cursors control)"
	[ "$(nth "$c" 1)" = 0 ] && echo "PASS control: cursor 0" || { echo "FAIL control: cursor $(nth "$c" 1), want 0"; fail=1; }
fi
if common typed; then
	c="$(cursors typed)"
	[ "$(nth "$c" 1)" = 2 ] && echo "PASS sdltext: cursor 2" || { echo "FAIL sdltext: cursor $(nth "$c" 1), want 2"; fail=1; }
	f="$(wmfocus typed)"
	if [ "$f" != main ]; then
		echo "NOTE wmchar: the main window had no keyboard focus ($f), so SDL drops the WM_CHAR text; cursor $(nth "$c" 2) not judged"
	else
		[ "$(nth "$c" 2)" = 3 ] && echo "PASS wmchar: cursor 3" || { echo "FAIL wmchar: cursor $(nth "$c" 2), want 3"; fail=1; }
	fi
fi
[ $fail = 0 ] && echo "text_input_check: PASS" || echo "text_input_check: FAIL"
exit $fail
