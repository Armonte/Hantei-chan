import sys, os, json, importlib.util, numpy as np
from PIL import Image
R='/mnt/c/dev/hantei-chan/wt/render-match3/tools/uni/s1_compare/'
spec=importlib.util.spec_from_file_location('v2m', R+'v2_model.py'); v2=importlib.util.module_from_spec(spec); spec.loader.exec_module(v2)
sh=v2.sh
case='mbtl_ctrl_chr001_p0f0'; gn=case+'_nb'; gd='/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2'
bw='/mnt/c/dev/hantei-chan/wt/render-match3/docs/audit_2026_10_14/evidence/compare/v2/editor_bw'
game=Image.open(f'{gd}/{gn}_game.png').convert('RGB'); meta=json.load(open(f'{gd}/{gn}_game.json'))
pl=Image.open(f'{gd}/{gn}_game_plate.png').convert('RGB')
ek=Image.open(f'{bw}/{case}_new_k.png'); ew=Image.open(f'{bw}/{case}_new_w.png')
org=sh.editor_origin(ek); sh.LIGHT=None; sh.REFINE=0
sh.game_vs_model(game,pl,ek,ew,v2.zoom_of(case),meta,org)
G,B,P,Q,m=sh.game_vs_model.last_arrays; xa,ya,xb,yb,gorg=sh.game_vs_model.last_window
print('fit',[tuple(round(x,3) for x in f) for f in sh.game_vs_model.last_fit], 'origin',gorg,'win',xa,ya,xb,yb)
# bg007 strip at x index: read
def strip(name):
    b=open(f'/mnt/c/dev/hantei-chan/gamedata/mbtl/bg/bg007/{name}','rb').read(); w=int.from_bytes(b[12:16],'little'); h=int.from_bytes(b[16:20],'little')
    a=np.frombuffer(b[20:20+w*h*4],np.uint8).reshape(h,w,4).astype(float); return a
xi=4096//2+(-40960>>7)
col=strip('stage_color.img')[:,xi]; spc=strip('stage_specular.img')[:,xi]; bk=strip('stage_bokashi_alpha.img')[0,xi,0]/255
print('x',xi,'col top/bot',col[0],col[1],'spec',spc[0],spc[1],'bokashi',bk)
rows=np.arange(ya,yb,dtype=float)[:,None]
h=gorg[1]-rows
op=(1-Q.mean(2))
opm=op>0.5
def boxmean(a,r=4):
    from numpy.lib.stride_tricks import sliding_window_view
    pad=np.pad(a,r); return sliding_window_view(pad,(2*r+1,2*r+1)).mean((-1,-2))
for order in ('rgb','bgr'):
  for H in (330.0, 1.0):
    for useb in (0,1):
      t=np.clip(h/H,0,1)
      pred=np.empty_like(G); 
      cnt=boxmean(opm.astype(float))
      for c in range(3):
        ci=c if order=='rgb' else 2-c
        base=col[1,ci]/255; hgt=(col[0,ci]-col[1,ci])/255
        sb=spc[1,ci]; sht=spc[0,ci]-spc[1,ci]
        C=base+hgt*t; S=sb+sht*t
        e=P[...,c]/np.maximum(op,1e-3)/255  # unpremultiplied colour
        p4=boxmean(np.where(opm,np.clip(e,0,1)**4,0))/np.maximum(cnt,1e-6)
        bo=useb*bk*p4*255*np.where(opm,1,cnt)
        pred[...,c]=P[...,c]*C+op*S+Q[...,c]*B[...,c]+bo
      pred=np.clip(pred,0,255); d=np.abs(G-pred)
      fit=[]
      for c in range(3):
        y=(pred[...,c]-Q[...,c]*B[...,c])[m]; X=np.stack([P[...,c][m],(1-Q[...,c])[m]],1)
        fit.append(tuple(np.round(np.linalg.lstsq(X,y,rcond=None)[0],3)))
      print(order,'H',H,'bokashi',useb,'mean|d| %.2f  >24: %.1f%%'%(d[m].mean(),((d.sum(2)>24)&m).sum()*100/m.sum()),'equiv k,c',fit)
print('G is pred?', G is pred, 'G mean', G[m].mean(), 'P mean', P[m].mean())
G2=np.asarray(game).astype(float)[ya:yb,xa:xb]; print('raw game crop mean', G2[m].mean(), 'diff to G', np.abs(G2-G).max())
t=np.clip(h/330.0,0,1); pred=np.empty_like(G); cnt=boxmean(opm.astype(float))
for c in range(3):
    C=col[1,c]/255+(col[0,c]-col[1,c])/255*t; S=spc[1,c]+(spc[0,c]-spc[1,c])*t
    e=P[...,c]/np.maximum(op,1e-3)/255; p4=boxmean(np.where(opm,np.clip(e,0,1)**4,0))/np.maximum(cnt,1e-6)
    pred[...,c]=P[...,c]*C+op*S+Q[...,c]*B[...,c]+bk*p4*255*np.where(opm,1,cnt)
pred=np.clip(pred,0,255); d=np.abs(G-pred).max(2)
print('check: pct50/90/99/max', np.percentile(d[m],[50,90,99,100]), 'n',m.sum())
