import numpy as np
from scipy.optimize import least_squares
import fit
from fit import tr, svf
def model(p,mode,f):
    out=0
    for k in range(3):
        f0,Q,a,b,c=p[5*k:5*k+5]
        lp,bp,hp=svf(f,f0,Q)
        if mode=='LPF': out=out+a*lp
        if mode=='BPF1': out=out+b*bp*(1,-1,1)[k]
        if mode=='BPF2': out=out+b*bp
        if mode=='HPF': out=out+c*hp
    if mode=='LPF': out=out/(1+1j*f/p[15])
    return 20*np.log10(np.abs(out)+1e-9)
def res(p): return np.concatenate([model(p,m,f)-d for m,(f,d) in tr.items()])
p0=np.concatenate([np.load('p.npy'),[12500]])
s=least_squares(res,p0)
p=s.x; print('rms',np.sqrt(np.mean(s.fun**2)).round(3)); 
for k in range(3): print(['LF','MF','HF'][k],np.round(p[5*k:5*k+5],4))
print('lp pole',p[15]); np.save('pf.npy',p)
for m,(f,d) in tr.items(): print(m,np.sqrt(np.mean((model(p,m,f)-d)**2)).round(2))
