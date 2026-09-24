#!/usr/bin/env python3
"""Tag-order analysis over all UNI2/MBTL HA6 files: reports tag pairs seen in
both orders (conflicts) and a linear order per block (used by the writer,
see src/ha6_enc.h ha6order)."""
import sys,glob,collections
sys.path.insert(0,'/mnt/c/dev/hantei-chan/wt/uni/tools/uni')
from ha6walk import walk
pairs=collections.Counter(); C=collections.Counter()
LAYER={'AFOF','AFAL','AFRG','AFAX','AFAY','AFAZ','AFAN','AFZM','AFPL','AFRT','AFTN'}|{'AFY%s'%c for c in '123789X'}
def norm(t):
    if t.startswith('AFD'): return 'AFD*'
    if t in('AFF1','AFF2','AFFL'): return 'AFF*'
    if t in('ASS1','ASS2'): return 'ASS*'
    if t.startswith('ATS') and t[3].isdigit(): return 'ATSn'
    if t in ('ASV0','ASVX'): return 'ASV*'
    if t.startswith('AFY'): return 'AFOF'
    return t
def addseq(k,s):
    for i in range(len(s)):
        for j in range(i+1,len(s)):
            if s[i]!=s[j]: pairs[(k,s[i],s[j])]+=1
ROOT=sys.argv[1] if len(sys.argv)>1 else "/mnt/c/dev/hantei-chan/gamedata"
for f in glob.glob(ROOT+"/*/data/**/*.[hH][aA]6",recursive=True):
    blk=None; seq=[]; layer=[]; frame=[]; boxes=[]
    for pid,tag,w,o in walk(open(f,'rb').read()):
        if tag=='AFST': blk='AF'; seq=[]; layer=None; continue
        if tag=='ASST': blk='AS'; seq=[]; continue
        if tag=='ATST': blk='AT'; seq=[]; frame.append('ATST'); continue
        if tag=='FSTR': frame=[]; boxes=[]; continue
        if tag=='FEND':
            addseq('FR',[norm(t) for t in frame]);
            # box order check
            prevk=None
            for b in boxes:
                if prevk and b<prevk: C['box_order_decreasing']+=1
                prevk=b
            C['frames']+=1
            continue
        if tag in('AFED','ASED','ATED'):
            if blk=='AF':
                if layer is not None: addseq('LAYER',layer)
                addseq('AFframe',[norm(t) for t in seq])
            else: addseq(blk,[norm(t) for t in seq])
            if blk=='AF': frame.append('AFST')
            if blk=='AS': frame.append('ASST')
            blk=None; continue
        if blk=='AF':
            if tag in('AFGX','AFGP'):
                if layer is not None: addseq('LAYER',layer)
                layer=[tag]
            elif tag in LAYER: 
                if layer is None: C['layer_tag_before_AFGX']+=1
                else: layer.append(norm(tag))
            else: seq.append(tag)
            continue
        if blk: seq.append(tag); continue
        if tag in('HRNM','HRNS'): boxes.append((0,w[0]))
        if tag in('HRAT','HRAS'): boxes.append((1,w[0]))
        if tag in ('HRNM','HRNS','HRAT','HRAS'): tag={'HRNS':'HRNM','HRAS':'HRAT'}.get(tag,tag)
        if tag in('EFTP','EFNO','EFPR','EFED','IFTP','IFPR','IFED'): continue
        frame.append(tag)
bad=0
for (k,a,b),n in sorted(pairs.items()):
    if (k,b,a) in pairs and a<b: print('CONFLICT',k,a,b,n,pairs[(k,b,a)]); bad+=1
print(C)
# print a total order per block
import graphlib
for k in ('LAYER','AFframe','AS','AT','FR'):
    g=graphlib.TopologicalSorter()
    for (kk,a,b),n in pairs.items():
        if kk==k and (k,b,a) not in pairs: g.add(b,a)
    try: print(k, list(g.static_order()))
    except Exception as e: print(k,'cycle',e)
