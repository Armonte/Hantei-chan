#!/bin/bash
# In-game proof of a CG manager edit saved to the UNI2-IM / MBTL-IM mod folder (src/cgm/cgm_texsrc.h ModFolderPath):
# fpan-im [boot] training (fpan-im tests/ingame/run_boot.sh steps) with the edited file installed in <game>\<IM>\Mods\<key> only while
# the game runs. Install, boot, capture and restore all happen inside gamecap's machine-wide game lock (the one run.sh takes), so no
# other agent's game session ever sees the edited file.
#   tools/cg/ingame_modfolder_boot.sh uni2|mbtl <edited file (WSL)> <key, e.g. data/chr003/chr003.cg> <evidence dir (WSL)> [p1] [p2]
set -u
G="$1"; SRC="$2"; KEY="$3"; EV="$4"; P1="${5:-3}"; P2="${6:-5}"
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
FB=/mnt/c/dev/fpan-im; GC=/mnt/c/dev/hantei-chan/tools/gamecap; LOCK=/tmp/claude-1000/game_session.lock
case "$G" in
uni2) GAMEDIR='C:\Program Files (x86)\Steam\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes';;
mbtl) GAMEDIR='C:\Program Files (x86)\Steam\steamapps\common\MELTY BLOOD TYPE LUMINA';;
*) echo "game: uni2 | mbtl"; exit 2;;
esac
TX="$ROOT/build/cgmtex.exe"
# gamecap captures need a Windows drive path: the case writes to C:\dev\hantei-chan\work\cgb_ev_<game>, copied to <evidence dir> afterwards
CEV=/mnt/c/dev/hantei-chan/work/cgb_ev_$G; rm -rf "$CEV"; mkdir -p "$CEV"
mkdir -p "$EV" "$FB/build/inject"; cp "$FB/build/bin/fpan-im.dll" "$FB/build/inject/fpan-im.dll"
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
CASE="$FB/tests/ingame/fpanim_${G}_boot_training.gcs"; TMPCASE="$FB/tests/ingame/.run_cgb_$(basename "$CASE")"
sed -e "s/^let P1CHR .*/let P1CHR $P1/" -e "s/^let P2CHR .*/let P2CHR $P2/" -e "s|^let EV .*|let EV $(wslpath -w "$CEV" | sed 's/\\/\\\\/g')|" "$CASE" > "$TMPCASE"
SRCW="$(wslpath -w "$(realpath "$SRC")")"
flock "$LOCK" bash -c '
	if tasklist.exe 2>/dev/null | grep -i -q -E "^(uni2|uni2_booty[^ ]*|MBTL|MBAA|mbacPC[^ ]*|hantei4[^ ]*|RBO[^ ]*)\.exe "; then echo "a game is already running:"; tasklist.exe | grep -i -E "^(uni2|MBTL|MBAA|RBO)"; exit 1; fi
	"$1" modsave "$2" "$3" "$4" "$5" || exit 1
	"$6" "$(wslpath -w "$7")" </dev/null; r=$?
	"$1" modrestore "$2" "$3" "$4"
	exit $r' _ "$TX" "$G" "$GAMEDIR" "$KEY" "$SRCW" "$GC/build/gamecap.exe" "$TMPCASE" 2>&1 | tee "$EV/gamecap_run.log"
rc=${PIPESTATUS[0]}
rm -f "$TMPCASE"
cp "$CEV"/*.png "$EV"/ 2>/dev/null
L=$(ls -t "$FB"/build/inject/logs/*.log 2>/dev/null | head -1); [ -n "$L" ] && cp "$L" "$EV/fpan-im.log"
exit $rc
