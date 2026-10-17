#!/bin/bash
# In-game before/after shots for docs/showcase_2026-10-16/ingame: fpan-im [boot] training (P1 = the edited character), an idle
# shot, then a 236+button special (gamecap pad only, no global input) shot every few frames. The edited files (any number,
# none = vanilla "before" run) are installed in <game>\<IM>\Mods\<key> only inside gamecap's game lock and restored before it
# is released (cgmtex modsave / modrestore, as tools/cg/ingame_modfolder_boot.sh).
#   tools/cg/showcase_ingame_boot.sh uni2|mbtl <out dir (WSL)> <button A|B|X|Y> [<key>=<file (WSL)>]...
# Pad X is the game's A button on the default UNI2 / MBTL pad layout (pad A is D, the Shield), so 236+A is button X.
set -u
G="$1"; EV="$2"; BTN="$3"; shift 3; PAIRS=("$@"); P1=3; P2=5
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; TX="$ROOT/build/cgmtex.exe"
FB=/mnt/c/dev/fpan-im; GC=/mnt/c/dev/hantei-chan/tools/gamecap; LOCK=/tmp/claude-1000/game_session.lock
case "$G" in
uni2) GAMEDIR='C:\Program Files (x86)\Steam\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes';;
mbtl) GAMEDIR='C:\Program Files (x86)\Steam\steamapps\common\MELTY BLOOD TYPE LUMINA';;
*) echo "game: uni2 | mbtl"; exit 2;;
esac
CEV=/mnt/c/dev/hantei-chan/work/showcase_ev_$G; rm -rf "$CEV"; mkdir -p "$CEV" "$EV" "$FB/build/inject"
cp "$FB/build/bin/fpan-im.dll" "$FB/build/inject/fpan-im.dll"
python3 - "$FB/build/inject/fpan-im.ini" "$P1" "$P2" <<'PY'
import os, re, sys
p, p1, p2 = sys.argv[1:]
lines = open(p).read().splitlines() if os.path.exists(p) else []
want = {"training": "1", "p1": p1, "p2": p2, "p1color": "-1", "p2color": "-1"}
out, sec = [], ""
for l in lines:
    m = re.match(r"\s*\[(.*)\]", l)
    if m: sec = m.group(1).strip().lower()
    if sec == "boot" and l.split("=")[0].strip().lower() in want: continue
    out.append(l)
if "[boot]" not in [l.strip().lower() for l in out]: out.append("[boot]")
i = [l.strip().lower() for l in out].index("[boot]")
out[i + 1:i + 1] = ["%s = %s" % kv for kv in want.items()]
open(p, "w").write("\n".join(out) + "\n")
PY
CASE="$FB/tests/ingame/fpanim_${G}_boot_training.gcs"; TMPCASE="$FB/tests/ingame/.run_showcase_$(basename "$CASE")"
EVW="$(wslpath -w "$CEV" | sed 's/\\/\\\\/g')"
{ sed -e "s/^let P1CHR .*/let P1CHR $P1/" -e "s/^let P2CHR .*/let P2CHR $P2/" -e "s|^let EV .*|let EV $EVW|" "$CASE" | sed '/^capture .*boot_battle/,$d'
  echo 'wait_frames 60'; echo 'capture ${EV}\idle.png'; echo 'wait_frames 7'; echo 'capture ${EV}\idle_b.png'
  echo 'pad_down DOWN'; echo 'wait_frames 3'; echo 'pad_down RIGHT'; echo 'wait_frames 3'; echo 'pad_up DOWN'; echo 'wait_frames 3'
  echo "pad_tap $BTN 3 0"; echo 'pad_up RIGHT'
  for f in 02 06 10 14 18 22 26 30 36 42 50 60; do echo 'wait_frames 4'; echo "capture \${EV}\\move_$f.png"; done
  echo 'status'; } > "$TMPCASE"
# modsave every pair, run, modrestore every pair (in reverse), all inside the lock
INST=(); for p in "${PAIRS[@]}"; do INST+=("${p%%=*}" "$(wslpath -w "$(realpath "${p#*=}")")"); done
flock "$LOCK" bash -c '
	TX=$1 G=$2 GD=$3 GCE=$4 CS=$5; shift 5
	if tasklist.exe 2>/dev/null | grep -i -q -E "^(uni2|uni2_booty[^ ]*|MBTL|MBAA|mbacPC[^ ]*|hantei4[^ ]*|RBO[^ ]*)\.exe "; then echo "a game is already running"; exit 1; fi
	keys=(); ok=1
	while [ $# -ge 2 ]; do "$TX" modsave "$G" "$GD" "$1" "$2" && keys+=("$1") || ok=0; shift 2; done
	r=1; [ $ok = 1 ] && { "$GCE" "$CS" </dev/null; r=$?; }
	for k in "${keys[@]}"; do "$TX" modrestore "$G" "$GD" "$k"; done
	exit $r' _ "$TX" "$G" "$GAMEDIR" "$GC/build/gamecap.exe" "$(wslpath -w "$TMPCASE")" "${INST[@]}" 2>&1 | tee "$EV/gamecap_run.log"
rc=${PIPESTATUS[0]}
rm -f "$TMPCASE"; cp "$CEV"/*.png "$EV"/ 2>/dev/null
exit $rc
