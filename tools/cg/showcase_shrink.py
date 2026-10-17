#!/usr/bin/env python3
"""Copy the chosen in-game frames to docs/showcase_2026-10-16/ingame as JPGs no wider than 1280 px.
  showcase_shrink.py <out dir> <game> <before dir> <after dir> <move frame, e.g. move_06>"""
import sys
from PIL import Image
out, g, bdir, adir, mv = sys.argv[1:]
for tag, d in (("before", bdir), ("after", adir)):
    for kind, f in (("idle", "idle"), ("move", mv)):
        im = Image.open("%s/%s.png" % (d, f)).convert("RGB"); im.thumbnail((1280, 1280))
        p = "%s/%s_%s_%s.jpg" % (out, g, tag, kind); im.save(p, quality=88); print(p, im.size)
