"""Exporta a csv/ formas de onda de S4c/S4d (casos fallidos): 5000 pts en la ventana indicada. Uso: python raw2csv_S4.py"""
import numpy as np
from spicelib import RawRead
SIG = ['V(vout)', 'V(vext)', 'V(pgood)', 'V(g6)', 'I(L1)', 'I(Vid)', 'V(vin)', 'V(ss)']

def export(base, t0, t1, npts=5000):
    r = RawRead(base + '.raw'); t = np.abs(r.get_trace('time').get_wave(0)); tg = np.linspace(t0, t1, npts)
    cols = [np.interp(tg, t, r.get_trace(n).get_wave(0)) for n in SIG]
    np.savetxt(f'csv/{base}.csv', np.column_stack([tg] + cols), delimiter=',', header='time,' + ','.join(SIG), comments='')
    print(base, npts)

export('S4c_sin', 2.9e-3, 6e-3)
export('S4cd_con', 2.9e-3, 17e-3)
