#!/bin/bash
# Captures of every Authoring workspace view against the MOCK dev link (no game). NOTE: each capture OPENS A VISIBLE
# editor window for a few seconds (it steals focus): run it only when the user agreed. docs/HANTEI_AUTHORING_MODE.md
# docs/HANTEI_AUTHORING_MODE.md §6.4 H5. Usage: tools/authoring/capture_authoring.sh [outdir]
#   outdir default: /mnt/c/dev/hantei-chan/docs/authoring/hantei
# Each capture runs gonptechan.exe in build/capwork with a fresh hanteichan.ini (1600x1000, no detached OS windows, so
# every tool window is inside the captured main window), a mock-dll serving the fixture sidecar tree
# (tests/fixtures/authoring/tag, copied), and the game folder C:\games\mbaacc_dev READ-ONLY (data\ .txt/.HA6/.pal,
# Bg\BgList.ini, GRP\ art). The fixture copy's md5 is checked after every capture: opening / drawing never writes.
set -u
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-/mnt/c/dev/hantei-chan/docs/authoring/hantei}
B=$REPO/build
W=$B/capwork
GAME='C:\games\mbaacc_dev'
mkdir -p "$OUT" "$W"
rm -rf "$W/tag"; cp -r "$REPO/tests/fixtures/authoring/tag" "$W/tag"
WROOT=$(wslpath -w "$W/tag")
( cd "$W/tag" && find . -type f | sort | xargs md5sum ) > "$W/md5_before.txt"
fail=0
MOCKJOB=
mock_start() {   # args: mock-dll options. Only this job is ever killed (its own $!).
	rm -f "$W/mock.out"
	( cd "$W" && exec "$B/game_link_cli.exe" mock-dll 90 root "$WROOT" "$@" > "$W/mock.out" 2>&1 ) &
	MOCKJOB=$!
	for _ in $(seq 1 50); do grep -q "pid" "$W/mock.out" 2>/dev/null && break; sleep 0.1; done
	MOCKPID=$(grep -o "pid [0-9]*" "$W/mock.out" | head -1 | cut -d' ' -f2)
}
mock_stop() { [ -n "$MOCKJOB" ] && kill "$MOCKJOB" 2>/dev/null; wait "$MOCKJOB" 2>/dev/null; MOCKJOB=; sleep 0.3; }
shot() {   # name, then editor args
	local name=$1; shift
	printf '[Other settings][]\nsizeX=1600\nsizeY=1000\nMaximized=0\nDetachableWindows=0\n' > "$W/hanteichan.ini"
	rm -f "$W/hanteichan_authoring.ini" "$W/imgui.ini"
	# --authoring-pid pins the link to the mock (another agent's MBAA.exe may be running: never touch it); 0 = no link
	( cd "$W" && timeout 90 "$B/gonptechan.exe" --tool authoring --authoring-pid "${MOCKPID:-0}" \
		--authoring-game "$GAME" --authoring-tag-root "$WROOT" "$@" --capture "$(wslpath -w "$OUT/$name.png")" > "$W/$name.log" 2>&1 )
	( cd "$W/tag" && find . -type f | sort | xargs md5sum ) > "$W/md5_after.txt"
	if ! cmp -s "$W/md5_before.txt" "$W/md5_after.txt"; then echo "FAIL $name: the fixture tree changed"; fail=1; fi
	[ -f "$OUT/$name.png" ] && echo "ok   $name.png" || { echo "FAIL $name: no png"; fail=1; }
}
TAG_SETUP=$("$B/game_link_cli.exe" setup-hex tag tohno/c/3 v.sion/c/0 sion/c/0 miyako/f/0 stage 16 assist 1 2 236A assist 2 5 236C | tr -d '\r')
DUO_SETUP=$("$B/game_link_cli.exe" setup-hex tag tohno/c/3 v.sion/c/0 maids/c/0 miyako/f/0 stage 16 | tr -d '\r')
OPEN="--open $GAME\\data\\shiki_1.txt"

mock_start
shot 01_setup_tag        --authoring-link --authoring-tab Setup --authoring-setup "$TAG_SETUP"
shot 02_setup_banned_duo --authoring-link --authoring-tab Setup --authoring-setup "$DUO_SETUP"
shot 03_setups           --authoring-link --authoring-tab Setups --authoring-setup "$TAG_SETUP"
shot 04_tuning_global    --authoring-link --authoring-tab Tuning --authoring-view global --authoring-setup "$TAG_SETUP"
shot 05_tuning_char_full $OPEN --authoring-link --authoring-tab Tuning --authoring-char shiki --authoring-layer f --authoring-setup "$TAG_SETUP"
shot 06_tuning_char_all  $OPEN --authoring-link --authoring-tab Tuning --authoring-char shiki --authoring-layer all --authoring-setup "$TAG_SETUP"
shot 07_tuning_ab        --authoring-link --authoring-tab Tuning --authoring-view global --authoring-ab 1 --authoring-setup "$TAG_SETUP"
shot 08_hud_layout       --authoring-link --authoring-tab HUD --authoring-setup "$TAG_SETUP"
shot 10_log              --authoring-link --authoring-tab Log --authoring-setup "$TAG_SETUP"
mock_stop
mock_start skew cancelWindowTicks env regenPerTick
shot 09_live_mismatch    --authoring-link --authoring-tab Live --authoring-setup "$TAG_SETUP"
mock_stop
mock_start session
shot 11_session_lock     --authoring-link --authoring-tab Tuning --authoring-char shiki --authoring-setup "$TAG_SETUP"
mock_stop
mock_start session host between
shot 12_session_host_between --authoring-link --authoring-tab Tuning --authoring-view global --authoring-setup "$TAG_SETUP"
mock_stop
mock_start rev1
shot 13_rev1_dll         --authoring-link --authoring-tab Setup --authoring-setup "$TAG_SETUP"
mock_stop
mock_start hash 12345678
shot 14_lever_table_mismatch --authoring-link --authoring-tab Tuning --authoring-view global --authoring-setup "$TAG_SETUP"
mock_stop
MOCKPID=4294967295   # a pid that does not exist: the link never finds a game
shot 15_not_linked       --authoring-tab Setup --authoring-setup "$TAG_SETUP"
cp "$W/md5_before.txt" "$OUT/fixture_md5.txt"
exit $fail
