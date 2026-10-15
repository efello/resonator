import numpy as np
from scipy.optimize import least_squares
from digitize import trace
from fit import svf
p=np.load('p.npy')
f,d=trace('resonator 02-RANGE.png'); k=(d>-27)&(d<17.5)&(f>20); f,d=f[k],d[k]
def m(f0s):
    out=0
    for k in range(3):
        _,Q,a,b,c=p[5*k:5*k+5]; lp,bp,hp=svf(f,f0s[k],Q); out=out+b*bp
    return 20*np.log10(abs(out)+1e-9)
s=least_squares(lambda x:m(x)-d,[62,330,1500],loss='soft_l1')
print(np.round(s.x,1),np.sqrt(np.mean((m(s.x)-d)**2)).round(2))
print('ratios',s.x[0]/s.x[0], 304/s.x[0],1233/s.x[1],7488/s.x[2])
