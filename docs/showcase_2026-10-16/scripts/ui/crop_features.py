#!/usr/bin/env python3
"""crop_features.py <shot dir> <features dir>: cut each feature window out of the full-frame --ui-script capture.
The boxes are the windows' rects in the 1884x961 client area that run_features.sh produces (ab_shot.sh 1900x1000 layout)."""
import sys
from PIL import Image

BOXES = {   # name: (left, top, right, bottom); the squirrel hover box also covers the tooltip, the game link box the sprite
    'feat_squirrel_hover': (58, 58, 1130, 756),
    'feat_squirrel_problems': (58, 58, 1060, 756),
    'feat_stage3d_uni2_align': (58, 58, 961, 700),
    'feat_stage3d_mbtl_align': (58, 58, 961, 700),
    'feat_cgm_uni2_palettes': (58, 58, 1261, 880),
    'feat_cgm_uni2_textures': (58, 58, 1261, 880),
    'feat_gamelink_panel': (60, 58, 1100, 700),
    'pcmod_export_window': (98, 68, 1081, 950),
    'pcmod_export_done': (98, 68, 1081, 950),
}
src, dst = sys.argv[1], sys.argv[2]
for name, box in BOXES.items():
    Image.open(f'{src}/{name}.png').convert('RGB').crop(box).save(f'{dst}/{name}.png', optimize=True)
    print(name, box)
