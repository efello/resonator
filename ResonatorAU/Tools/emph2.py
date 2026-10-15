import numpy as np
from PIL import Image
from scipy.optimize import least_squares
from digitize import px2f,py2db,D
import fit
from fit import svf
p=np.load('p.npy')
names={'LPF':'resonator 03-EMPH-LPF.png','BPF1':'resonator 03-EMPH-BPF1.png','BPF2':'resonator 03-EMPH-BPF2.png','HPF':'resonator 03-EMPH-HPF.png'}
def trace_rng(name,lo,hi):
    im=np.array(Image.open(D+name).convert('L')).astype(float)
    F=[];Db=[]
    for x in range(58,872):
        ys=np.where((im[50:545,x]>=lo)&(im[50:545,x]<=hi))[0]
        if len(ys): F.append(px2f(x));Db.append(py2db(np.median(ys)+50))
    return np.array(F),np.array(Db)
def model_q(Qs,mode,f,p=p):
    out=0
    for k in range(3):
        f0,_,a,b,c=p[5*k:5*k+5]
        lp,bp,hp=svf(f,f0,Qs[k])
        out=out+{'LPF':a*lp,'BPF1':b*bp*(1,-1,1)[k],'BPF2':b*bp,'HPF':c*hp}[mode]
    return 20*np.log10(np.abs(out)+1e-9)
if __name__=='__main__':
  for lvl,(lo,hi) in {'mid':(98,135),'low':(166,186)}.items():
    data={m:trace_rng(n,lo,hi) for m,n in names.items()}
    for m in data:
        f,d=data[m]; k=(d>-27)&(f>(115 if m=='HPF' else 0)); data[m]=(f[k],d[k])
    def res(q): return np.concatenate([model_q(q,m,f)-d for m,(f,d) in data.items()])
    s=least_squares(res,[2,2,2],bounds=([0.2]*3,[20]*3))
    print(lvl,'Q',np.round(s.x,3),'rms',np.sqrt(np.mean(s.fun**2)).round(2))
