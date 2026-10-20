#!/bin/bash
# tools/render_parity/text_input_check.sh [exe] [out-dir]
#
# Visible type-to-search check (P0 fix 2, item 2). The old WndProc got WM_CHAR in
# every window whether or not an ImGui text field was active, so the archive
# browser's type-to-search (io.InputQueueCharacters) worked; under SDL a
# character only becomes SDL_EVENT_TEXT_INPUT while SDL text input is on for the
# window, which imgui_impl_sdl3 turns off whenever no field wants it. This runs
# the editor VISIBLY (AB_VISIBLE=1, through tools/ui/ab_shot.sh) on a folder of
# four characters (alpha, bravo, charlie, delta), cursor on alpha, three times:
#
#   control  nothing typed                           -> cursor 0 (alpha)
#   sdltext  "c" as a real SDL_EVENT_TEXT_INPUT       -> cursor 2 (charlie)
#            pushed at the main window (SDL_PushEvent)
#   wmchar   "c" as WM_CHAR posted to the app's own   -> cursor 2, when the window has the keyboard focus
#            main window (SDL's window procedure       (SDL drops text for an unfocused window; the run
#            turns it into text input)                  then reports it instead of failing)
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
		printf 'browse %s\\dir\nwaitidle\nwait 5\nkey Down\nwait 5\ntextinput\n' "$OUTW"
		for l in "$@"; do printf '%s\n' "$l"; done
		printf 'wait 10\ninfo\ntextinput\ncapture %s/%s.png\nquit\n' "$OUTM" "$name"
	} > "$OUT/${name}_script.txt"
	AB_VISIBLE=1 AB_TIMEOUT=60 AB_EXE="$EXE" AB_WORK=/mnt/c/dev/hantei-chan/work/ab_text_input_check \
		bash "$D/tools/ui/ab_shot.sh" "$OUT/${name}_script.txt" > "$OUT/${name}_run.log" 2>&1
	echo "== $name: $(head -1 "$OUT/${name}_run.log")"
	sed 's/^/   /' "$OUT/${name}_run.log" | tail -n +2
}
cursor() { grep -o 'cursor=-\?[0-9]*' "$OUT/$1_run.log" | tail -1 | cut -d= -f2; }
focus() { grep '^textinput' "$OUT/$1_run.log" | tail -1 | grep -o 'keyboard_focus=[a-z]*' | cut -d= -f2; }
check() {  # check <name> <expected cursor> [focus-required]
	local name="$1" want="$2" c f
	c="$(cursor "$name")"; f="$(focus "$name")"
	[ "$(head -1 "$OUT/${name}_run.log")" = "exit 0" ] || { echo "FAIL $name: $(head -1 "$OUT/${name}_run.log")"; fail=1; return; }
	[ -s "$OUT/${name}.png" ] || { echo "FAIL $name: no capture"; fail=1; }
	grep -q '^textinput main=1 want_text=0 ' "$OUT/${name}_run.log" \
		|| { echo "FAIL $name: SDL text input is off for the main window with no text field active"; fail=1; }
	if [ "${3:-}" = focus ] && [ "$f" != main ]; then
		echo "NOTE $name: the main window has no keyboard focus ($f), so SDL drops the WM_CHAR text; cursor $c not judged"
		return
	fi
	[ "$c" = "$want" ] && echo "PASS $name: cursor $c" || { echo "FAIL $name: cursor $c, want $want"; fail=1; }
}

run control
run sdltext "sdltext c"
run wmchar "wmchar c"
check control 0
check sdltext 2
check wmchar 2 focus
[ $fail = 0 ] && echo "text_input_check: PASS" || echo "text_input_check: FAIL"
exit $fail
