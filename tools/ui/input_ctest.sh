#!/bin/bash
# tools/ui/input_ctest.sh <gonptechan.exe> <character .ha6> <work dir on C:> : ctest `input_text`.
#
# Archive browser type-to-search through the real text paths (P0 fix 2), headless, no desktop input:
#   sdltext  - an SDL_EVENT_TEXT_INPUT pushed at the main window (what SDL makes of WM_CHAR) reaches ImGui's
#              character queue, and the browser jumps to the entry starting with the typed letter;
#   control  - nothing typed: the cursor stays on the first entry;
# and SDL text input is on for the main window with no text field active (main.cpp KeepTextInputActive), which is
# what lets SDL turn a real WM_CHAR into text input at all. The WM_CHAR path itself (wmchar) needs the window to
# have the keyboard focus, which a --headless window never has: tools/render_parity/text_input_check.sh runs it
# visibly.
# The browsed folder holds four copies of the fixture (alpha, bravo, charlie, delta); typing "c" moves the cursor
# from 0 (alpha) to 2 (charlie). The work dir must be Windows-visible (a C: folder): browsing the ext4 build folder
# through \\wsl.localhost lists the entries without names.
set -u
EXE="$1"; CHAR="$2"; WORK="$3"
rm -rf "$WORK"; mkdir -p "$WORK/dir"
for n in alpha bravo charlie delta; do cp "$CHAR" "$WORK/dir/$n.ha6"; done
cp "$EXE" "$WORK/gonptechan.exe"
W() { wslpath -w "$1"; }
fail() { echo "FAIL: $*"; exit 1; }
run() {  # run <name> <script lines...>: the browser on dir, cursor on alpha, then the lines, then info + textinput
	local name="$1"; shift
	{
		printf 'browse %s\\dir\nwaitidle\nwait 5\nkey Down\nwait 5\n' "$(W "$WORK")"
		for l in "$@"; do printf '%s\n' "$l"; done
		printf 'wait 5\ninfo\ntextinput\nquit\n'
	} > "$WORK/$name.txt"
	(cd "$WORK" && timeout 60 ./gonptechan.exe --headless --ui-script "$(W "$WORK")\\$name.txt" </dev/null >/dev/null 2>&1)
	local rc=$?
	[ $rc -eq 0 ] || fail "$name: exit $rc"
}
cursor() { grep -o 'cursor=-\?[0-9]*' "$WORK/$1.txt.log" | tail -1 | cut -d= -f2; }
check() {  # check <name> <expected cursor>
	local c; c="$(cursor "$1")"
	echo "$1: cursor=$c $(grep '^textinput' "$WORK/$1.txt.log" | tail -1)"
	[ "$c" = "$2" ] || fail "$1: cursor $c, want $2 (log: $(tr '\n' '|' < "$WORK/$1.txt.log"))"
	grep -q '^textinput main=1 want_text=0 ' "$WORK/$1.txt.log" || fail "$1: SDL text input is off for the main window with no text field active"
}
run control
run sdltext "sdltext c"
check control 0
check sdltext 2
echo "OK"
