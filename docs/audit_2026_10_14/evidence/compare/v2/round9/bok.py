import numpy as np, sys
from numpy.lib.stride_tricks import sliding_window_view
def boxmean(a, r=4): return sliding_window_view(np.pad(a, r), (2*r+1, 2*r+1)).mean((-1, -2))
for f in ['p435','ctrl']:
  z=np.load(f+'.npz'); G,lp,m,Q,P=z['G'],z['lp'],z['m'],z['Q'],z['P']
  op=1-Q.mean(2); opm=op>0.5; cnt=boxmean(opm.astype(float))
  d=G-lp; inn=m&(op>0.98)
  print(f, 'base |d| fp %.2f'%np.abs(d[m]).mean())
  for pw in (1,2,4):
    X4=np.stack([boxmean(np.where(opm, np.clip(P[...,c]/np.maximum(op,1e-3)/255,0,1)**pw,0))/np.maximum(cnt,1e-6)*255 for c in range(3)],2)
    for mode in ('add','blend'):
      Xr = X4 if mode=='add' else X4-lp
      tot=[]; co=[]
      for c in range(3):
        A=np.stack([np.ones(inn.sum()), Xr[...,c][inn]],1); y=d[...,c][inn]
        k,*_=np.linalg.lstsq(A,y,rcond=None); co.append(np.round(k,3))
      # apply with shared alpha (mean of channels) and per-channel offsets
      a=np.mean([k[1] for k in co])
      nd=d.copy()
      for c in range(3): nd[...,c]-= co[c][0]*0+a*Xr[...,c]*op
      print('  pow%d %-5s alpha %.3f  offs %s  -> fp |d| %.2f (interior %.2f -> %.2f)'%(pw,mode,a,[k[0] for k in co],np.abs(nd[m]).mean(),np.abs(d[inn]).mean(),np.abs(nd[inn]).mean()))
