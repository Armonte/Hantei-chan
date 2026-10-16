#!/bin/bash
# In-game showcase recolour (docs/showcase_2026-10-16/ingame): the modder's round trip through the editor's CG manager,
# driven by --ui-script (no real input):
#   export: "CG + .pat textures" tab -> Export PNG of the idle (pattern 0) sprites and one .pat texture; Palettes tab -> Export
#           colours (.act) of palette 0 (colour 1, what fbmod's boot picks).
#   (recolour the .act and the texture PNG with tools/cg/hue_shift.py)
#   import: Palettes tab -> Import colours (the shifted .act) -> "Save .pal to mod folder"; "CG + .pat textures" tab ->
#           Import PNG (the shifted texture) -> "Save to mod folder". The game folder is a scratch folder, never the live game.
#   tools/cg/showcase_recolour.sh export|import uni2|mbtl <chr> <pat texture id>
# Work dir: C:\dev\hantei-chan\work\showcase_ingame_<game> (export/, edit/, gamedir/<IM>/Mods/data/<chr>/...).
# HC_ROOT picks the build (default: this worktree).
set -u
PH="$1"; G="$2"; C="$3"; T="$4"
D="${HC_ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}"; TX="$D/build/cgmtex.exe"
W=/mnt/c/dev/hantei-chan/work/showcase_ingame_$G; SRC=/mnt/c/dev/hantei-chan/gamedata/$G/data
case $G in uni2) GI=3;; mbtl) GI=4;; *) echo "game: uni2|mbtl"; exit 2;; esac
w() { wslpath -w "$1"; }
M="$(wslpath -m "$W")"; S="$W/$PH.txt"
if [ "$PH" = export ]; then
	rm -rf "$W"; mkdir -p "$W/fx/data" "$W/export" "$W/edit" "$W/gamedir"
	cp -r "$SRC/$C" "$W/fx/data/"; cp "$SRC"/BaseData.* "$W/fx/data/" 2>/dev/null
fi
mapfile -t IDS < <("$TX" pattern-ids "$(w "$SRC/$C/$C.ha6")" 0 | tr -d '\r')
{ echo "openany $M/fx/data/$C/${C}_0.txt"; echo "wait 150"; echo "cgm open"; echo "wait 30"
  echo "cgm tab CG + .pat textures"; echo "wait 10"; echo "cgm texgame $GI"; echo "cgm texdir $(w "$W/gamedir")"
  if [ "$PH" = export ]; then
	for i in "${IDS[@]}"; do echo "cgm texsel 0 $i"; echo "wait 3"; echo "cgm path $M/export/idle_$i.png"; echo "cgm press Export PNG...##cgmtb"; echo "wait 4"; done
	echo "cgm texsel 1 $T"; echo "wait 10"; echo "cgm path $M/export/pat_$T.png"; echo "cgm press Export PNG...##cgmtb"; echo "wait 20"
	echo "cgm status"; echo "capture $M/gui_export_texture.png"
	echo "cgm tab Palettes"; echo "wait 10"; echo "cgm set palBank 0"; echo "cgm set palNumber 0"; echo "wait 5"
	echo "cgm path $M/export/pal0.act"; echo "cgm press Export colours..."; echo "wait 5"; echo "cgm status"; echo "capture $M/gui_export_palette.png"
  else
	echo "cgm tab Palettes"; echo "wait 10"; echo "cgm set palBank 0"; echo "cgm set palNumber 0"; echo "wait 5"
	echo "cgm path $M/edit/pal0.act"; echo "cgm press Import colours..."; echo "wait 10"; echo "cgm status"; echo "capture $M/gui_import_palette.png"
	echo "cgm press Save .pal to mod folder"; echo "wait 10"; echo "cgm status"
	echo "cgm tab CG + .pat textures"; echo "wait 10"; echo "cgm texsel 1 $T"; echo "wait 10"
	echo "cgm path $M/edit/pat_$T.png"; echo "cgm press Import PNG...##cgmtb"; echo "wait 30"; echo "cgm status"
	echo "capture $M/gui_import_texture.png"
	echo "cgm press Save to mod folder##cgmtb"; echo "wait 60"; echo "cgm status"; echo "capture $M/gui_saved.png"
	echo "cgm texsel 0 ${IDS[0]}"; echo "wait 10"; echo "capture $M/gui_idle_recoloured.png"
  fi
  echo "quit"; } > "$S"
AB_WORK="$W/ab" AB_TIMEOUT=900 "$D/tools/ui/ab_shot.sh" "$S" > "$W/$PH.log" 2>&1; rc=$?
grep -a -i 'status\|error\|unknown' "$W/$PH.log" | tail -12
ls -la "$W/export" "$W/gamedir" 2>/dev/null | tail -5; find "$W/gamedir" -type f 2>/dev/null
exit $rc
