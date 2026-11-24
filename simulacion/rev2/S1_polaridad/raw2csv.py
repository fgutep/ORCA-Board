"""Exporta a csv/ formas de onda diezmadas (5000 pts, remuestreo uniforme en la ventana indicada) de los casos
fallidos o al limite, leyendo el .raw con spicelib. Correr en esta carpeta tras simular: python raw2csv.py"""
import re, numpy as np
from spicelib import RawRead

def steps(log):
    txt = open(log, encoding='latin-1').read()
    return [dict(re.findall(r'(\w+)=(\S+)', m.rstrip('Â°'))) for m in re.findall(r'^\.step (.*)$', txt, re.M)]

def export(base, pick, t0, t1, outdir='csv', npts=5000):
    import os; os.makedirs(outdir, exist_ok=True)
    st = steps(base + '.log'); raw = RawRead(base + '.raw')
    names = [n for n in raw.get_trace_names() if n != 'time']
    for k, p in enumerate(st):
        if not pick(p): continue
        t = np.abs(raw.get_trace('time').get_wave(k)); tg = np.linspace(t0, t1, npts)
        cols = [np.interp(tg, t, raw.get_trace(n).get_wave(k)) for n in names]
        tag = '_'.join(f'{a}{b}' for a, b in p.items() if a != 'temp' or True)
        fn = f'{outdir}/{base}_{tag}.csv'
        np.savetxt(fn, np.column_stack([tg] + cols), delimiter=',', header='time,' + ','.join(names), comments='')
        print(fn, len(tg))

# S1d: barrido de BAT con C_IN cargado (VBUS arrastrado a negativo); casos sin carga (ild=0), ventana 5.9-10 ms
export('S1d_transitorio', lambda p: float(p['ild']) == 0, 5.9e-3, 10e-3)
