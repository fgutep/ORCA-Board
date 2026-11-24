import sys, numpy as np
from spicelib import RawRead
for f in sys.argv[1:]:
    try:
        r=RawRead(f); t=np.abs(r.get_trace('time').get_wave(0)); print(f, round(t[-1]*1e3,4),'ms', len(t))
    except Exception as e: print(f,'err',str(e)[:60])
