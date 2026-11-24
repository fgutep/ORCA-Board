"""S5: lee S5.log (.meas), valida el log, calcula la tabla (aritmetica directa sobre .meas), y como
contraste (estimacion) la mediana de 50 ns sobre S5.raw; exporta csv/ de ventanas de evento.
Uso: python postproceso.py   (en esta carpeta, tras simular)"""
import re, os, math, numpy as np

log = open('S5.log', encoding='latin-1').read()
valid = ('Total elapsed time' in log) and ('relax' not in log.lower()) and ('caret' not in log.lower())
print('LOG VALIDO:', valid, '| tolerance relaxed:', 'relax' in log.lower())

M = {}
for l in log.splitlines():
    m = re.match(r'^(\w+): (.*)$', l)
    if not m: continue
    n, r = m.groups()
    if re.match(r'^x\d_', n):                       # cruces: tiempo tras " AT "
        mm = re.search(r' AT\s+([-\d.eE+]+)', r)
        if mm: M[n] = float(mm.group(1))
        continue
    r = re.sub(r'\s+FROM .*$', '', r)
    mm = re.search(r'=\s*(-?\d[\d.eE+-]*)\s*$', r)
    if mm: M[n] = float(mm.group(1))

T = {1: (3.5e-3, 5.0e-3), 2: (5.0e-3, 6.5e-3)}
BANDS = {'b20': 2.0, 'b10': 1.0, 'b05': 0.5}
res = {}
for n, (t0, t1) in T.items():
    g = lambda k: M.get(f'{k}{n}')
    out = dict(vpre=g('vpre'), drift_mV=M.get(f'vpre{n}_drift'),
               iload_pre=g('iload_pre'), iload_end=g('iload_end'), ilmin_pre=g('ilmin_pre'), ilavg_pre=g('ilavg_pre'),
               ilmin_end=g('ilmin_end'), ilavg_end=g('ilavg_end'),
               dnu=g('dnu'), dov=g('dov'), dnu_r=M.get(f'dnu{n}_r'), dov_r=M.get(f'dov{n}_r'),
               vmin=M.get(f'vmin{n}_f'), vmax=M.get(f'vmax{n}_f'),
               ripple_pre_pp=(M.get(f'vmaxpre{n}_f') - M.get(f'vminpre{n}_f')) if M.get(f'vmaxpre{n}_f') is not None else None,
               vend_shift=g('vend_shift'), il_pk_ev=g('il_pk_ev'), il_vy_ev=g('il_vy_ev'))
    for b, B in BANDS.items():
        hi = sorted(M[k] for k in M if re.fullmatch(rf'x{n}_{b}hi_\d', k) and M[k] < t1 - 1e-12)
        lo = sorted(M[k] for k in M if re.fullmatch(rf'x{n}_{b}lo_\d', k) and M[k] < t1 - 1e-12)
        allx = hi + lo
        end_in = abs(M.get(f'vend{n}_sl', float('nan')) / M[f'vpre{n}'] - 1) * 100 < B if f'vpre{n}' in M else None
        trunc = len(hi) >= 8 or len(lo) >= 8
        out[b] = dict(n_hi=len(hi), n_lo=len(lo), tset=(max(allx) - t0) if allx else 0.0, exited=bool(allx),
                      end_inside=end_in, truncated=trunc, cycles=min(math.ceil(len(hi) / 2), math.ceil(len(lo) / 2)))
    res[n] = out

def fmt(x, f='{:.4g}'):
    return 'n/d' if x is None else f.format(x)

for n in (1, 2):
    o = res[n]
    print(f'\n== Evento {n} ({"1->4 A" if n == 1 else "4->1 A"}) ==')
    for k in ('vpre', 'drift_mV', 'iload_pre', 'iload_end', 'ilavg_pre', 'ilmin_pre', 'ilavg_end', 'ilmin_end', 'ripple_pre_pp',
              'vmin', 'vmax', 'dnu', 'dov', 'dnu_r', 'dov_r', 'vend_shift', 'il_pk_ev', 'il_vy_ev'):
        print(f'  {k:14s} {fmt(o[k])}')
    for b in BANDS:
        d = o[b]
        print(f'  banda {BANDS[b]}%: sale={d["exited"]} tset={d["tset"]*1e6:.1f} us  n_hi={d["n_hi"]} n_lo={d["n_lo"]} ciclos={d["cycles"]} termina_dentro={d["end_inside"]} truncado={d["truncated"]}')

# --- contraste (estimacion): mediana de 50 ns sobre el .raw
try:
    from spicelib import RawRead
    from scipy.signal import medfilt
    raw = RawRead('S5.raw')
    t = np.abs(raw.get_trace('time').get_wave()); v = raw.get_trace('V(vout)').get_wave()
    dt = 1e-9; w = 51                                  # 51 muestras de 1 ns ~ 50 ns
    print('\n== Contraste (estimacion): mediana 50 ns sobre V(vout) ==')
    for n, (t0, t1) in T.items():
        tg = np.arange(t0 - 0.2e-3, t1, dt)
        vm = medfilt(np.interp(tg, t, v), w)
        pre = vm[(tg >= t0 - 0.2e-3) & (tg < t0)]
        post = vm[tg >= t0]
        vp = M[f'vpre{n}']
        print(f'  ev{n}: undershoot {(vp - post.min()) / vp * 100:.3f} %  overshoot {(post.max() - vp) / vp * 100:.3f} %  (vpre de .meas {vp:.4f} V)')
    # csv de ventanas
    os.makedirs('csv', exist_ok=True)
    names = [x for x in raw.get_trace_names() if x != 'time']
    tg = np.linspace(3.4e-3, 6.5e-3, 6000)
    cols = [np.interp(tg, t, raw.get_trace(x).get_wave()) for x in names]
    np.savetxt('csv/S5.csv', np.column_stack([tg] + cols), delimiter=',', header='time,' + ','.join(names), comments='')
    print('csv/S5.csv', len(tg), 'puntos')
except Exception as e:
    print('contraste/csv no ejecutado:', e)
