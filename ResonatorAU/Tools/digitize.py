import numpy as np, glob, os
from PIL import Image
D='/home/claude/curves/'
X0,X1=53.5,875.0   # 20Hz..20kHz
def px2f(x): return 20*10**((x-X0)/(X1-X0)*3)
def py2db(y): return 20-(y-48)/9.94
def trace(name, dark=40, mid=None):
    im=np.array(Image.open(D+name).convert('L')).astype(float)
    fs=[];db=[]
    for x in range(58,872):
        col=im[50:545,x]
        lo,hi=(0,dark) if mid is None else mid
        ys=np.where((col>=lo)&(col<=hi))[0]
        if len(ys)==0: continue
        # pick contiguous cluster; use median
        fs.append(px2f(x)); db.append(py2db(np.median(ys)+50))
    return np.array(fs),np.array(db)
if __name__=='__main__':
    for n in sorted(os.listdir(D)):
        f,d=trace(n); print(n,len(f),d.min().round(1),d.max().round(1))
