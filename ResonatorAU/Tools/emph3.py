import numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from scipy.optimize import least_squares
from emph2 import *
fig,ax=plt.subplots(2,2,figsize=(13,8)); out={}
for lvl,(lo,hi),col in [('mid',(98,135),'b'),('low',(168,186),'g')]:
    data={m:trace_rng(n,lo,hi) for m,n in names.items()}
    for m in data:
        f,d=data[m]; k=(d>-27)&(f>(115 if m=='HPF' else 0)); data[m]=(f[k],d[k])
    def res(q): return np.concatenate([model_q(q,m,f)-d for m,(f,d) in data.items()])
    best=None
    for q0 in ([2,3,2],[0.5,0.8,0.6],[1,1,1]):
        s=least_squares(res,q0,bounds=([0.2]*3,[20]*3),loss='soft_l1',f_scale=1.0)
        if best is None or s.cost<best.cost: best=s
    r=res(best.x); print(lvl,np.round(best.x,3),'median abs err',np.median(np.abs(r)).round(2)); out[lvl]=best.x
    for a,(m,(f,d)) in zip(ax.ravel(),data.items()):
        a.semilogx(f,d,col+'.',ms=2); ff=np.geomspace(20,20000,600); a.semilogx(ff,model_q(best.x,m,ff),col+'-'); a.set_title(m); a.set_ylim(-30,20)
plt.savefig('emph.png',dpi=60); np.save('qs.npy',np.array([p[[1,6,11]],out['mid'],out['low']]))
