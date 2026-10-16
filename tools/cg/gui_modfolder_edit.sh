#!/bin/bash
# GUI half of the in-game mod-folder proof: the editor's CG manager "CG + .pat textures" tab (--ui-script, no real input) imports
# striped PNGs into the images that pattern 0 draws and presses "Save to mod folder" with a scratch game folder. The file the GUI
# wrote is what tools/cg/ingame_modfolder_boot.sh then installs (inside the game lock) and boots.
#   tools/cg/gui_modfolder_edit.sh uni2|mbtl <chr, e.g. chr003> <evidence dir (WSL)>
# Out: /mnt/c/dev/hantei-chan/work/cgb_gamedir_<game>/<IM>/Mods/data/<chr>/<chr>.cg
set -u
G="$1"; C="$2"; EV="$3"
D="$(cd "$(dirname "$0")/../.." && pwd)"; TX="$D/build/cgmtex.exe"
W=/mnt/c/dev/hantei-chan/work/cgb_gui_$G; GD=/mnt/c/dev/hantei-chan/work/cgb_gamedir_$G; SRC=/mnt/c/dev/hantei-chan/gamedata/$G/data
rm -rf "$W" "$GD"; mkdir -p "$W/fx/data" "$W/png" "$GD" "$EV"
cp -r "$SRC/$C" "$W/fx/data/"; cp "$SRC"/BaseData.* "$W/fx/data/" 2>/dev/null
case $G in uni2) GI=3; IM=UNI2-IM;; mbtl) GI=4; IM=MBTL-IM;; esac
# the striped pictures: cgmtex stripe on a copy (same layer), exported per image as the PNGs the GUI imports
w() { wslpath -w "$1"; }
"$TX" stripe "$(w "$SRC/$C/$C.cg")" "$(w "$SRC/$C/$C.ha6")" "$(w "$SRC/$C/$C.pal")" "$(w "$W/striped.cg")" 0-0 > "$EV/gui_stripe_src.log"
mapfile -t IDS < <("$TX" pattern-ids "$(w "$SRC/$C/$C.ha6")" 0 | tr -d '\r')
S="$W/script.txt"; M="$(wslpath -m "$W")"
{ echo "openany $M/fx/data/$C/${C}_0.txt"; echo "wait 150"; echo "cgm open"; echo "wait 30"; echo "cgm tab CG + .pat textures"; echo "wait 10"
  echo "cgm texgame $GI"; echo "cgm texdir $(w "$GD")"
  for i in "${IDS[@]}"; do
    "$TX" export "$(w "$W/striped.cg")" "$i" "$(w "$W/png/$i.png")" > /dev/null
    echo "cgm texsel 0 $i"; echo "wait 3"; echo "cgm path $M/png/$i.png"; echo "cgm press Import PNG...##cgmtb"; echo "wait 4"
  done
  echo "cgm status"; echo "cgm texsel 0 ${IDS[0]}"; echo "wait 10"; echo "capture $M/gui_after_import.png"
  echo "cgm press Save to mod folder##cgmtb"; echo "wait 60"; echo "cgm status"; echo "capture $M/gui_saved.png"; echo "quit"; } > "$S"
AB_WORK="$W/ab" AB_TIMEOUT=900 "$D/tools/ui/ab_shot.sh" "$S" > "$EV/gui_${G}.log" 2>&1
cp "$W"/gui_*.png "$EV"/ 2>/dev/null
OUT="$GD/$IM/Mods/data/$C/$C.cg"; ls -la "$OUT" >> "$EV/gui_${G}.log" 2>&1
[ -f "$OUT" ] && echo "GUI wrote $OUT" || { echo "GUI did not write $OUT"; exit 1; }
