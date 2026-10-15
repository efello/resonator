import numpy as np
from scipy.optimize import least_squares
from digitize import trace
tr={}
tr['LPF']=trace('resonator 01-MAX-01.png')
tr['BPF1']=trace('resonator 01-MAX-02.png')
tr['BPF2']=trace('resonator 01-MAX-03.png')
f,d=trace('resonator 03-EMPH-HPF.png'); m=f>115; tr['HPF']=(f[m],d[m])
# drop deep notch clipped region (<-27 dB)
for k in tr:
    f,d=tr[k]; m=d>-27.5; tr[k]=(f[m],d[m])
def svf(f,f0,Q):
    s=1j*f/f0
    den=s*s+s/Q+1
    return 1/den, s/den, s*s/den   # LP, BP(unnormalized, peak=Q), HP
def model(p,mode,f):
    out=0
    for k in range(3):
        f0,Q,a,b,c=p[5*k:5*k+5]
        lp,bp,hp=svf(f,f0,Q)
        out=out+{'LPF':a*lp,'BPF1':b*bp*(1,-1,1)[k],'BPF2':b*bp,'HPF':c*hp}[mode]
    return 20*np.log10(np.abs(out)+1e-9)
def res(p):
    r=[]
    for m,(f,d) in tr.items(): r.append(model(p,m,f)-d)
    return np.concatenate(r)
p0=[]
for f0,Q in [(300,7.5),(1200,7.5),(7500,7.5)]: p0+=[f0,Q,0.5,0.85,1.0]
lb=[];ub=[]
for k in range(3): lb+=[f0*0.8 for f0 in [(300,1200,7500)[k]]]+[1,-5,-5,-5]; ub+=[(300,1200,7500)[k]*1.25,30,5,5,5]
s=least_squares(res,p0,bounds=(lb,ub))
p=s.x
print('rms dB',np.sqrt(np.mean(s.fun**2)))
for k in range(3): print(['LF','MF','HF'][k],np.round(p[5*k:5*k+5],3))
np.save('p.npy',p)
