import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
def boxmean(a, r=4): return sliding_window_view(np.pad(a, r), (2*r+1, 2*r+1)).mean((-1, -2))
def X4(P,Q,r=4):
  op=1-Q.mean(2); opm=op>0.5; cnt=boxmean(opm.astype(float),r)
  return np.stack([boxmean(np.where(opm, np.clip(P[...,c]/np.maximum(op,1e-3)/255,0,1)**4,0),r)/np.maximum(cnt,1e-6)*255 for c in range(3)],2), op
D={f:np.load(f+'.npz') for f in ['p435','ctrl']}
for r in (4,):
 for a in (0.06,0.08,0.10,0.12,0.15):
  out=[]
  # offsets from ctrl so that ctrl interior mean shift is removed
  z=D['ctrl']; x,op=X4(z['P'],z['Q'],r); off=-(a*x*op[...,None])[z['m']&(op>0.98)].mean(0)
  for f,z in D.items():
    x,op=X4(z['P'],z['Q'],r)
    pred=np.clip(z['lp']+(a*x+off)*op[...,None],0,255)
    out.append('%s %.2f'%(f,np.abs(z['G']-pred)[z['m']].mean()))
  print('r',r,'a',a,'off',np.round(off,2),out)
