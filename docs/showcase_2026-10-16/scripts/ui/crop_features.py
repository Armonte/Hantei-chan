#!/usr/bin/env python3
"""crop_features.py <shot dir> <features dir>: cut each feature window out of the full-frame --ui-script capture.
The boxes are the windows' rects in the 1884x961 client area that run_features.sh produces (ab_shot.sh 1900x1000 layout).
Since ui::FitNextWindow (layout sweep) the tool windows open centred on the work area; the pcmod export window keeps its spot."""
import os, sys
from PIL import Image

BOXES = {   # name: (left, top, right, bottom); the squirrel hover box also covers the tooltip, the game link box is the panel only (it no longer sits over the sprite)
    'feat_squirrel_hover': (440, 128, 1444, 834),
    'feat_squirrel_problems': (440, 128, 1444, 834),
    'feat_stage3d_uni2_align': (440, 98, 1444, 864),
    'feat_stage3d_mbtl_align': (440, 98, 1444, 864),
    'feat_cgm_uni2_palettes': (340, 68, 1544, 894),
    'feat_cgm_uni2_textures': (340, 68, 1544, 894),
    'feat_gamelink_panel': (620, 218, 1264, 742),
    'pcmod_export_window': (98, 68, 1081, 956),
    'pcmod_export_done': (98, 68, 1081, 956),
}
src, dst = sys.argv[1], sys.argv[2]
for name, box in BOXES.items():
    if not os.path.exists(f'{src}/{name}.png'): continue   # re-crop a subset (e.g. only the CG manager shots)
    Image.open(f'{src}/{name}.png').convert('RGB').crop(box).save(f'{dst}/{name}.png', optimize=True)
    print(name, box)
