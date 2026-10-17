import sys
from PIL import Image
# crop.py name x0 y0 x1 y1 -> $TMPD/crop_<name>.png
n=sys.argv[1]; b=tuple(int(v) for v in sys.argv[2:6])
Image.open(f'/mnt/c/dev/hantei-chan/work/i18nshots/i18n_ja_{n}.png').crop(b).save(f'/home/teo/dev/hantei-chan-wt/i18n_tmp/crop_{n}.png')
