import numpy as np, sys
sys.path.insert(0,'.')
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    from fit_final import model, tr
p=np.load('pf.npy'); names=['LPF','BPF1','BPF2','HPF']
fgrid=np.geomspace(20,20000,500)
for fs in (44100,48000):
    print('fs',fs,' max|err| dB 20Hz-10kHz (20k) vs analog model | rms vs MEASURED captures')
    res={}
    for line in open(f'plug_{fs}.txt'):
        if not line.startswith('IR'): continue
        t=line.split(); q,m,lat=int(t[1]),int(t[2]),int(t[3]); h=np.array(t[4:],float)
        H=np.abs(np.fft.rfft(h)); fr=np.fft.rfftfreq(len(h),1/fs)
        dB=20*np.log10(np.interp(fgrid,fr,H)+1e-12)
        ref=model(p,names[m],fgrid); ok=ref>-24
        e=np.abs(dB-ref)
        f,d=tr[names[m]]; dm=20*np.log10(np.interp(f,fr,H)+1e-12)
        res.setdefault(q,[]).append('%s %.2f(%.2f) rms %.2f'%(names[m],e[ok&(fgrid<10000)].max(),e[ok].max(),np.sqrt(np.mean((dm-d)**2))))
    for q in res: print(' ',['Eco 1x ','Std 2x  ','High 4x '][q],' | '.join(res[q]))
