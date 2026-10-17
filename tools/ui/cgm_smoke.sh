#!/bin/bash
# tools/ui/cgm_smoke.sh [1 2 3 4]: GUI smoke of the CG manager buttons through --ui-script only (tools/ui/ab_shot.sh: no real
# mouse / keyboard, captures read the app's own GL frame). Every script works on a fresh fixture copy under work/ab_s45/fx,
# never on a game install. Shots and logs go to docs/audit_2026_10_14/evidence/cg/gui/.
#   1 structure + "Save bank, its projects and this character" (MBTL chr000, 4 projects, shared BaseData)
#   2 palettes: pair move, new _p1.pal, in-place Save .pal (MBTL chr000)
#   3 PovertyCaster palette export (MBAACC akiha copy)
#   4 folders: plain cgtool import with an edited PNG, new bank from folder, export all (MBTL chr000)
D="$(cd "$(dirname "$0")/../.." && pwd)"
export AB_WORK=/mnt/c/dev/hantei-chan/work/ab_s45
FX=$AB_WORK/fx; EV="$D/docs/audit_2026_10_14/evidence/cg/gui"; T="$D/build/cgmtool.exe"
mkdir -p "$EV" "$AB_WORK/shots"
fixture() {
	rm -rf "$FX"; mkdir -p "$FX/mbtl/data" "$FX/mbaacc/data"
	cp -r /mnt/c/dev/hantei-chan/gamedata/mbtl/data/chr000 "$FX/mbtl/data/"
	cp /mnt/c/dev/hantei-chan/gamedata/mbtl/data/BaseData.HA6 "$FX/mbtl/data/"
	cp /mnt/c/games/mbaacc/data/akiha* /mnt/c/games/mbaacc/data/b_akiha* "$FX/mbaacc/data/"
	python3 -I - "$FX/new_sprite.png" <<'EOF'
import sys
from PIL import Image, ImageDraw
im = Image.new('RGBA', (64, 96), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
d.rectangle([4, 4, 59, 91], fill=(255, 0, 255, 255)); d.rectangle([16, 16, 47, 79], fill=(0, 255, 0, 255))
im.save(sys.argv[1])
EOF
	(cd "$FX" && find . -type f | sort | xargs sha1sum > "$AB_WORK/fx_before.sha1")
}
for s in ${@:-1 2 3 4}; do
	fixture
	if [ "$s" = 4 ]; then   # a plain cgtool export with one picture edited (image 10: a magenta box over its top-left quarter)
		"$T" export "$(wslpath -w "$FX/mbtl/data/chr000/chr000.cg")" "$(wslpath -w "$FX/plain")" --cgtool > /dev/null
		python3 -I - "$FX/plain" <<'EOF'
import sys, glob
from PIL import Image
f = sorted(glob.glob(sys.argv[1] + '/*_ID_10.png'))[0]
im = Image.open(f).convert('RGBA'); w, h = im.size; px = im.load()
bb = im.getbbox() or (0, 0, w, h)
for y in range(bb[1], (bb[1] + bb[3]) // 2):
	for x in range(bb[0], (bb[0] + bb[2]) // 2):
		if px[x, y][3]: px[x, y] = (255, 0, 255, 255)
im.save(f); print('edited', f)
EOF
	fi
	sc=$(ls "$D"/tools/ui/shots_cgm/cgm_${s}_*.txt)
	AB_TIMEOUT=900 "$D/tools/ui/ab_shot.sh" "$sc" > "$EV/cgm_${s}.log" 2>&1
	(cd "$FX" && find . -type f | sort | xargs sha1sum > "$AB_WORK/fx_after.sha1")
	{ echo "--- files changed / added by the run (sha1 before vs after):"; diff "$AB_WORK/fx_before.sha1" "$AB_WORK/fx_after.sha1" | grep '^[<>]' | grep -v '/plain/\|/export_gui/' ; } >> "$EV/cgm_${s}.log"
	if [ "$s" = 1 ]; then   # the GUI save against the original: every layer of every rewritable project file still shows the same picture
		O=/mnt/c/dev/hantei-chan/gamedata/mbtl/data/chr000; N="$FX/mbtl/data/chr000"; args=()
		for f in "$O"/*.ha6; do args+=("$(wslpath -w "$f")" "$(wslpath -w "$N/$(basename "$f")")"); done
		"$T" refs-compare "$(wslpath -w "$O/chr000.cg")" "$(wslpath -w "$N/chr000.cg")" "${args[@]}" >> "$EV/cgm_${s}.log" 2>&1
	fi
	if [ "$s" = 2 ]; then   # the pair move (3 <-> 4 with its alternate set 68 <-> 69) is what the in-place save wrote
		python3 -I - "$FX/mbtl/data/chr000" >> "$EV/cgm_${s}.log" <<'EOF'
import sys
d = sys.argv[1]; a = open(d + '/chr000.pal.bak', 'rb').read(); b = open(d + '/chr000.pal', 'rb').read()
P = lambda x, i: x[16 + i * 1024:16 + (i + 1) * 1024]
n = int.from_bytes(a[12:16], 'little'); ch = [i for i in range(n) if P(a, i) != P(b, i)]
ok = ch == [3, 4, 68, 69] and P(a, 3) == P(b, 4) and P(a, 4) == P(b, 3) and P(a, 68) == P(b, 69) and P(a, 69) == P(b, 68) and a[:16] == b[:16]
print('SECTION gui-pal-pair-move: %s | %d palettes, changed %s; _p1.pal == saved .pal: %s' % ('pass' if ok else 'FAIL', n, ch, open(d + '/chr000_p1.pal', 'rb').read() == b))
EOF
	fi
	if [ "$s" = 4 ]; then "$T" check "$(wslpath -w "$FX/built.cg")" | tail -1 >> "$EV/cgm_${s}.log"; ls "$FX/export_gui" | head -5 >> "$EV/cgm_${s}.log"; fi
	cp "$AB_WORK"/shots/cgm_${s}_*.png "$EV/" 2>/dev/null
	echo "== smoke $s: $(grep -c . "$EV/cgm_${s}.log") log lines"
done
