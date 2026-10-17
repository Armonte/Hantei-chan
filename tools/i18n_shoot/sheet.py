import sys
from PIL import Image
out=sys.argv[1]; cols=int(sys.argv[2]); ims=[Image.open(f) for f in sys.argv[3:]]
w=max(i.width for i in ims); h=max(i.height for i in ims); rows=(len(ims)+cols-1)//cols
S=Image.new('RGB',(w*cols,h*rows),(255,0,255))
for k,i in enumerate(ims): S.paste(i,((k%cols)*w,(k//cols)*h))
S.save(out)
