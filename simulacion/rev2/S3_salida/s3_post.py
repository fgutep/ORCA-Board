"""Post-proceso S3 (solo lectura de .log/.raw; no inventa numeros: tablas desde .meas, CSV desde .raw).
Uso:  python s3_post.py csv <caso.raw> <indice_paso_1based> <salida.csv>
      (importable: parse_log, read_raw, a_table)"""
import re, sys, math, numpy as np

def parse_log(path):
    txt = open(path, encoding='utf-8', errors='replace').read()
    steps = []
    for m in re.finditer(r'^\.step (.*)$', txt, re.M):
        d = {}
        for kv in m.group(1).split():
            k, v = kv.split('='); d[k.lower()] = float(v.replace('\u00b0', ''))
        steps.append(d)
    meas = {}
    for m in re.finditer(r'^Measurement: (\S+)\n(.*?)(?=\n\n|\Z)', txt, re.M | re.S):
        vals = []
        for r in m.group(2).splitlines()[1:]:
            c = r.split('\t')
            if len(c) < 2: c = r.split()
            try: vals.append(float(c[1]))
            except Exception: vals.append(None)      # 'failed' = no ocurrio
        meas[m.group(1).lower()] = vals
    return steps, meas

def read_raw(path):
    b = open(path, 'rb').read()
    key = 'Binary:\n'.encode('utf-16le'); i = b.index(key)
    hdr = b[:i].decode('utf-16le'); data = b[i + len(key):]
    L = hdr.splitlines()
    nv = int([l for l in L if l.startswith('No. Variables')][0].split(':')[1])
    npts = int([l for l in L if l.startswith('No. Points')][0].split(':')[1])
    flags = [l for l in L if l.startswith('Flags')][0]
    k = L.index('Variables:'); names = [L[k + 1 + j].split('\t')[2] for j in range(nv)]
    if 'double' in flags:
        arr = np.frombuffer(data, dtype=np.dtype([('v', '<f8', (nv,))]), count=npts)['v']
    else:
        r = np.frombuffer(data, dtype=np.dtype([('t', '<f8'), ('v', '<f4', (nv - 1,))]), count=npts)
        arr = np.column_stack([r['t'], r['v']]).astype(float)
    arr[:, 0] = np.abs(arr[:, 0])
    cut = [0] + list(np.where(np.diff(arr[:, 0]) < 0)[0] + 1) + [len(arr)]
    return [{n.lower(): arr[a:z, j] for j, n in enumerate(names)} for a, z in zip(cut[:-1], cut[1:])]

def decimate(run, n=5000, sigs=None, t0=None, t1=None):
    t = run['time']; sigs = sigs or [k for k in run if k != 'time']
    m = np.ones(len(t), bool)
    if t0 is not None: m &= t >= t0
    if t1 is not None: m &= t <= t1
    idx = np.where(m)[0]
    act = np.zeros(len(idx))
    for s in sigs:
        y = run[s][idx]; rg = (y.max() - y.min()) or 1.0
        act[1:] += np.abs(np.diff(y)) / rg
    act = act / (act.sum() or 1) * 0.7 + np.diff(np.r_[t[idx[0]], t[idx]]) / ((t[idx[-1]] - t[idx[0]]) or 1) * 0.3
    c = np.cumsum(act); c = c / c[-1]
    pick = np.unique(np.searchsorted(c, np.linspace(0, 1, n)))
    pick = idx[np.clip(pick, 0, len(idx) - 1)]
    return pick

def write_csv(path, run, n=5000, t0=None, t1=None):
    sigs = ['v(vint)', 'v(vext)', 'v(vdr)', 'v(g6)', 'v(pgood)', 'v(fb)', 'i(vid)', 'i(vsns)']
    sigs = [s for s in sigs if s in run]
    pick = decimate(run, n, sigs, t0, t1)
    with open(path, 'w') as f:
        f.write('t_s,' + ','.join(sigs) + ',vgs_q6\n')
        for i in pick:
            f.write(f"{run['time'][i]:.9g}," + ','.join(f"{run[s][i]:.6g}" for s in sigs) + f",{run['v(g6)'][i]-run['v(vint)'][i]:.6g}\n")
    return len(pick)

# ---- interpolacion de utilizacion SOA en el ancho de pulso equivalente t_eq = E/Ppk ----
TW = [100e-6, 1e-3, 10e-3, 40e-3, 100e-3]
def u_at(teq, us):
    if teq is None or not np.isfinite(teq): return None
    if teq <= TW[0]: return us[0]
    if teq >= TW[-1]: return us[-1]
    for i in range(len(TW) - 1):
        if TW[i] <= teq <= TW[i + 1]:
            f = (math.log(teq) - math.log(TW[i])) / (math.log(TW[i + 1]) - math.log(TW[i]))
            return us[i] + f * (us[i + 1] - us[i])        # interpolacion lineal en log t

if __name__ == '__main__':
    if sys.argv[1] == 'csv':
        runs = read_raw(sys.argv[2]); r = runs[int(sys.argv[3]) - 1]
        print(write_csv(sys.argv[4], r), 'puntos')

# ------------------------------------------------------------------ tablas markdown desde los .log
def _g(m, k, i):
    v = m.get(k)
    return None if v is None or i >= len(v) else v[i]

def a_rows(log, extra_keys=('cgd', 'rg', 'cq7', 'rq7')):
    steps, m = parse_log(log)
    rows = []
    for i, s in enumerate(steps):
        gets = lambda k: _g(m, k, i)
        n_pgf = sum(1 for k in ('pgf1', 'pgf2', 'pgf3', 'pgf4', 'pgf5') if gets(k) is not None)
        n_reop = sum(1 for k in ('goff1', 'goff2', 'goff3', 'goff4', 'goff5') if gets(k) is not None)
        n_recl = sum(1 for k in ('gon2', 'gon3', 'gon4', 'gon5', 'gon6') if gets(k) is not None)
        us = [gets(k) for k in ('mxu100u', 'mxu1m', 'mxu10m', 'mxu40m', 'mxudc')]
        teq = gets('teq'); u = u_at(teq, us) if None not in us else None
        rows.append(dict(s=s, ipk=gets('ipk'), ilim=gets('ilim'), drop=gets('drop'), vmin=gets('vmin'), fbmin=gets('fbmin'),
                         n_pgf=n_pgf, n_reop=n_reop, n_recl=n_recl, tfull=gets('tfull'), ppk=gets('ppk'), e=gets('eq6'),
                         teq=teq, u=u, vgsfin=gets('vgsfin'), vfin=gets('vfin')))
    return rows

def verdict_a(r):
    ok_drop = r['drop'] is not None and r['drop'] <= 5.0
    ok_pg = r['n_pgf'] == 0
    ok_re = r['n_reop'] == 0 and r['n_recl'] == 0
    return ok_drop, ok_pg, ok_re

def soa_flag(u):
    if u is None: return 'n/d'
    return 'PASA' if u <= 0.7 else ('AL LIMITE' if u <= 1.0 else 'FALLA')

def a_table(log, keys=None, bwk=None):
    out = ['| V_OUT | C_carga | ' + ('param | ' if keys else '') + 'I pico Q6 [A] | caida VOUT_INT [%] | PGOOD cae (n) | reapert./recierres Q6 | t encendido [ms] | P pico [W] | E Q6 [mJ] | t_eq [us] | util SOA(t_eq) | caida | PGOOD | reconm. | SOA |',
           '|' + '---|' * (14 + (1 if keys else 0))]
    for r in a_rows(log):
        s = r['s']
        if bwk is not None and abs(s.get('bwk', 1) - bwk) > 1e-3: continue
        ok_d, ok_p, ok_r = verdict_a(r)
        kv = ' / '.join(f"{k}={s[k]:g}" for k in (keys or []) if k in s)
        f = lambda x, p=3: ('n/d' if x is None else f'{x:.{p}g}')
        out.append(f"| {s['vn']:g} V | {s['cl']*1e6:g} uF | " + (f'{kv} | ' if keys else '') +
                   f"{f(r['ipk'])} | {f(r['drop'])} | {r['n_pgf']} | {r['n_reop']}/{r['n_recl']} | "
                   f"{f(None if r['tfull'] is None else (r['tfull']-1e-3)*1e3)} | {f(r['ppk'])} | {f(None if r['e'] is None else r['e']*1e3)} | "
                   f"{f(None if r['teq'] is None else r['teq']*1e6)} | {f(r['u'])} | "
                   f"{'PASA' if ok_d else 'FALLA'} | {'PASA' if ok_p else 'FALLA'} | {'PASA' if ok_r else 'FALLA'} | {soa_flag(r['u'])} |")
    return '\n'.join(out)

def b_table(log):
    steps, m = parse_log(log)
    f = lambda x, p=3: 'n/d' if x is None else f'{x:.{p}g}'
    has_d = 'idpk' in m
    out = ['| V_OUT | L carga | ' + ('C gd | ' if 'cgd' in (steps[0] if steps else {}) else '') + 'V_DS max Q6 [V] | VOUT_EXT min [V] | sobreimpulso VOUT_INT [%] | t V_GS=-2.5 V [ms] | t I<10% [ms] | E Q6 [mJ] | P pico [W] | t_eq [ms] | util SOA(t_eq) | ' + ('I pico diodo [A] | ' if has_d else '') + 'V_DS<32 V | SOA |',
           '|' + '---|' * (12 + (1 if has_d else 0) + (1 if (steps and 'cgd' in steps[0]) else 0))]
    for i, s in enumerate(steps):
        g = lambda k: _g(m, k, i)
        us = [g(k) for k in ('mxu100u', 'mxu1m', 'mxu10m', 'mxu40m', 'mxudc')]
        e, pk = g('eq6'), g('ppk'); teq = None if (e is None or not pk) else e / pk
        u = u_at(teq, us) if None not in us else None
        vds = g('vdsmax'); ok = 'n/d' if vds is None else ('PASA' if vds < 28.8 else ('AL LIMITE' if vds < 32 else 'FALLA'))
        t = lambda k: None if g(k) is None else (g(k) - 2e-3) * 1e3
        L = s['ll']; Ls = f'{L*1e6:g} uH' if L < 1e-3 else f'{L*1e3:g} mH'
        out.append(f"| {s['vn']:g} V | {Ls} | " + (f"{s['cgd']*1e9:g} nF | " if 'cgd' in s else '') + f"{f(vds)} | {f(g('vextmin'))} | {f(g('ovs'))} | {f(t('tgs'))} | {f(t('toff10'))} | {f(None if e is None else e*1e3)} | {f(pk)} | {f(None if teq is None else teq*1e3)} | {f(u)} | " +
                   (f"{f(g('idpk'))} | " if has_d else '') + f"{ok} | {soa_flag(u)} |")
    return '\n'.join(out)

def c_table(log):
    steps, m = parse_log(log); f = lambda x, p=3: 'n/d' if x is None else f'{x:.{p}g}'
    out = ['| V_OUT | T [C] | V_GS Q6 [V] | V_GS Q7 [V] | V_DS Q6 [mV] | I Q6 [A] | P Q6 [W] | Rds eq [mohm] | P zener [mW] | P R 4.7k LED2 [mW] | |V_GS Q6| 5-15 V | P<=0.5 W |', '|' + '---|' * 12]
    for i, s in enumerate(steps):
        g = lambda k: _g(m, k, i)
        ok1 = 'PASA' if 5 <= abs(g('vgs6')) <= 15 else 'FALLA'; ok2 = 'PASA' if g('p6') <= 0.5 else 'FALLA'
        out.append(f"| {s['vn']:g} V | {s['temp']:g} | {f(g('vgs6'))} | {f(g('vgs7'))} | {f(g('vds6')*1e3)} | {f(g('id6'))} | {f(g('p6'))} | {f(g('rds6'))} | {f(g('pz')*1e3)} | {f(g('pl2')*1e3)} | {ok1} | {ok2} |")
    return '\n'.join(out)

def d_table(log):
    steps, m = parse_log(log); f = lambda x, p=3: 'n/d' if x is None else f'{x:.{p}g}'
    out = ['| V_OUT | t PGOOD cae [ms] | t a 5 V [s] | t a 2 V [s] | t a 1 V [s] | P bleeder reg [W] | P total reg [W] (bleeder+FB+LED2) |', '|---|---|---|---|---|---|---|']
    for i, s in enumerate(steps):
        g = lambda k: _g(m, k, i)
        out.append(f"| {s['vn']:g} V | {f(g('tpgl')*1e3)} | {f(g('t5'))} | {f(g('t2'))} | {f(g('t1'))} | {f(g('pbl'))} | {f(g('ptot'))} |")
    return '\n'.join(out)

def e_table(log):
    steps, m = parse_log(log); f = lambda x, p=3: 'n/d' if x is None else f'{x:.{p}g}'
    out = ['| V_BAT | t a 10 V [s] | t a 5 V [s] | t a 3 V [s] | t a 2 V [s] | t a 1 V [s] | t a 0.5 V [s] | LED1 < 1 mA a [s] | LED1 < 10 uA a [s] (V) |', '|---|---|---|---|---|---|---|---|---|']
    for i, s in enumerate(steps):
        g = lambda k: _g(m, k, i)
        out.append(f"| {s['vb']:g} V | {f(g('t10'))} | {f(g('t5'))} | {f(g('t3'))} | {f(g('t2'))} | {f(g('t1'))} | {f(g('t05'))} | {f(g('tled1m'))} | {f(g('tled10u'))} ({f(g('vled10u'))} V) |")
    return '\n'.join(out)

def screen_table(log, keys):
    """Resumen por valor de remedio: cuantas de las 9 combinaciones (V_OUT x C_carga) cumplen cada criterio."""
    from collections import OrderedDict
    g = OrderedDict()
    for r in a_rows(log):
        k = tuple(r['s'][x] for x in keys if x in r['s']); g.setdefault(k, []).append(r)
    fmt = lambda x: (f'{x*1e9:g} nF' if x < 1e-5 else f'{x*1e6:g} uF') if False else x
    out = ['| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |', '|---|---|---|---|---|---|---|---|']
    for k, rs in g.items():
        n = len(rs); d = sum(verdict_a(r)[0] for r in rs); pg = sum(verdict_a(r)[1] for r in rs); re_ = sum(verdict_a(r)[2] for r in rs)
        al = sum(all(verdict_a(r)) for r in rs); so = sum((r['u'] or 9) <= 1 for r in rs)
        bad = ', '.join(f"{r['s']['vn']:g} V/{r['s']['cl']*1e6:g} uF" for r in rs if not all(verdict_a(r)))
        lab = ', '.join(f"{x}={v:.3g}" for x, v in zip([x for x in keys if x in rs[0]['s']], k))
        out.append(f"| {lab} | {d}/{n} | {pg}/{n} | {re_}/{n} | {al}/{n} | {so}/{n} | {max(r['drop'] for r in rs):.3g} | {bad or '-'} |")
    return '\n'.join(out)
