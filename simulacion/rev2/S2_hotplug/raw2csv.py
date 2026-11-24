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

# S2a (25.2 V, 9 casos; todos >= 25.6 V = FALLA o AL LIMITE), ventana 0.95-2.5 ms (flanco + oscilacion)
import numpy as np
from spicelib import RawRead
def vpk(base):
    raw = RawRead(base + '.raw'); out = []
    for k in range(len(steps(base + '.log'))):
        t = np.abs(raw.get_trace('time').get_wave(k)); v = raw.get_trace('V(vbus)').get_wave(k)
        out.append(v[t >= 1e-3].max())
    return out
pk = vpk('S2a_hotplug_25V'); st = steps('S2a_hotplug_25V.log')
export('S2a_hotplug_25V', lambda p: pk[st.index(p)] >= 25.6, 0.95e-3, 2.5e-3)
# S2d: remedio (snubber 470 uF + 0.1 ohm), solo el peor caso
pk2 = vpk('S2d_remedio_grid'); st2 = steps('S2d_remedio_grid.log'); w = max(pk2)
export('S2d_remedio_grid', lambda p: pk2[st2.index(p)] == w, 0.95e-3, 2.5e-3)
