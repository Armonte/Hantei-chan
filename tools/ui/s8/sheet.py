#!/usr/bin/env python3
# tools/ui/s8/sheet.py <evidence/s8 dir>: before | after side-by-side sheets (sheet_<case>.png), both at view zoom 1.
# SION pairs with after_sion_p65_f0_shield_if51.png: after_sion_p65_f0_shield.png was taken without zoomview (zoom 3).
import os, sys
from PIL import Image, ImageDraw

d = sys.argv[1]
PAIRS = [
    ("gchaos_p100_f10_arcdrive", "before_gchaos_p100_f10_arcdrive.png", "after_gchaos_p100_f10_arcdrive.png"),
    ("gchaos_p0_f0_boxes", "before_gchaos_p0_f0_boxes.png", "after_gchaos_p0_f0_boxes.png"),
    ("gchaos_p64_f2_parts_ef", "before_gchaos_p64_f2_parts_ef.png", "after_gchaos_p64_f2_parts_ef.png"),
    ("sion_p65_f0_shield", "before_sion_p65_f0_shield.png", "after_sion_p65_f0_shield_if51.png"),
]
for name, b, a in PAIRS:
    ib, ia = (Image.open(os.path.join(d, f)).convert("RGB") for f in (b, a))
    h = max(ib.height, ia.height)
    s = Image.new("RGB", (ib.width + ia.width + 8, h + 24), (32, 32, 32))
    s.paste(ib, (0, 24)); s.paste(ia, (ib.width + 8, 24))
    g = ImageDraw.Draw(s)
    g.text((6, 6), f"before: {b} (2026-10-04 exe, old roster, zoom 1)", fill=(255, 255, 255))
    g.text((ib.width + 14, 6), f"after: {a} (S8 build, roster_v2, zoom 1)", fill=(255, 255, 255))
    s = s.resize((1600, round(s.height * 1600 / s.width)), Image.LANCZOS)
    out = os.path.join(d, f"sheet_{name}.png")
    s.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(out, optimize=True)
    print(out, s.size)
