import numpy as np, subprocess, sys
def ir(fs,mode,emph,k,law=0):
    out=subprocess.run(['./dsp_test',str(fs),str(mode),str(emph),*map(str,k),str(law)],capture_output=True,text=True).stdout
    return np.array(out.split(),float)
sys.path.insert(0,'.')
# analog reference from fitted params (same constants as header)
from fit_final import model
p=np.load('pf.npy')
names=['LPF','BPF1','BPF2','HPF']
fgrid=np.geomspace(20,20000,400)
print('max |err| dB (20Hz-20kHz) / (20Hz-10kHz)   vs measured-fit analog prototype')
for fs in (88200,96000,176400,192000):
    row=[]
    for mode in range(4):
        h=ir(fs,mode,1.0,(1,1,1)); H=np.fft.rfft(h); fr=np.fft.rfftfreq(len(h),1/fs)
        dB=20*np.log10(np.abs(np.interp(fgrid,fr,np.abs(H)))+1e-12)
        ref=model(p,names[mode],fgrid)
        e=np.abs(dB-ref); 
        # ignore deep notch bottoms where ref<-24
        m=ref>-24
        row.append('%s %.2f/%.2f'%(names[mode],e[m].max(),e[m&(fgrid<10000)].max()))
    print(fs,' | '.join(row))
