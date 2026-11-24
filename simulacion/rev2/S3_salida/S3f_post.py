"""S3f post-proceso: tablas desde .meas de los .log (importa s3_post, sin modificarlo)."""
import sys, s3_post as p
f = lambda x, n=3: 'n/d' if x is None else f'{x:.{n}g}'
def eng(R): return 'fija' if R is None else (f'{R/1e3:g} k' if R < 1e9 else 'ninguna')
def b_table(log, t0=2e-3):
    st, m = p.parse_log(log); dfw = 'idpk' in m
    out = ['| V_OUT | L | Rrst | V_DS max [V] | VOUT_EXT min [V] | t V_GS=-2.5 V [ms] | t I<10% [ms] | E Q6 [mJ] | P pico [W] | t_eq [ms] | util SOA | ' + ('I pico D [A] | ' if dfw else '') + 'V_DS<32 | SOA |', '|' + '---|' * (12 + dfw)]
    for i, s in enumerate(st):
        g = lambda k: p._g(m, k, i)
        us = [g(k) for k in ('mxu100u', 'mxu1m', 'mxu10m', 'mxu40m', 'mxudc')]
        e, pk = g('eq6'), g('ppk'); teq = None if (e is None or not pk) else e / pk
        u = p.u_at(teq, us) if None not in us else None
        v = g('vdsmax'); ok = 'n/d' if v is None else ('PASA' if v < 28.8 else ('AL LIMITE' if v < 32 else 'FALLA'))
        t = lambda k: None if g(k) is None else (g(k) - t0) * 1e3
        L = s['ll']; Ls = f'{L*1e6:g} uH' if L < 1e-3 else f'{L*1e3:g} mH'
        out.append(f"| {s['vn']:g} V | {Ls} | {eng(s.get('rst', s.get('rstg')))} | {f(v)} | {f(g('vextmin'))} | {f(t('tgs'))} | {f(t('toff10'))} | {f(None if e is None else e*1e3)} | {f(pk)} | {f(None if teq is None else teq*1e3)} | {f(u)} | " + (f"{f(g('idpk'))} | " if dfw else '') + f"{ok} | {p.soa_flag(u)} |")
    return '\n'.join(out)
if __name__ == '__main__':
    print(b_table(sys.argv[1]))

def rec_rows(log, TOPEN=40e-3):
    st, m = p.parse_log(log); out = []
    for i, s in enumerate(st):
        g = lambda k: p._g(m, k, i)
        trec = TOPEN + s['gap']
        n_pg = sum(1 for k in ('pgf1', 'pgf2', 'pgf3') if g(k) is not None)
        n_re = sum(1 for k in ('goff1', 'goff2') if g(k) is not None)
        us = [g(k) for k in ('mxu100u', 'mxu1m', 'mxu10m', 'mxu40m', 'mxudc')]
        teq = g('teq'); u = p.u_at(teq, us) if None not in us else None
        tf = None if g('tfull') is None else (g('tfull') - trec) * 1e3
        okd = g('drop') is not None and g('drop') <= 5.0
        out.append(dict(s=s, vds=g('vdsrec'), vgs=g('vgsrec'), vm=g('vmrec'), vext=g('vextrec'), ipk=g('ipk'), drop=g('drop'), n_pg=n_pg, n_re=n_re, tf=tf, e=g('eq6'), u=u, ok=(okd, n_pg == 0, n_re == 0), vgsfin=g('vgsfin'), vfin=g('vfin')))
    return out
def rec_table(log):
    out = ['| V_OUT | C_carga | GAP [ms] | V_DS al recierre [V] | V_GS al recierre [V] | V(m)-V(vint) [V] | I pico [A] | caida [%] | PGOOD cae (n) | reaperturas (n) | t V_DS<0.5 V [ms] | E [mJ] | util SOA | caida | PGOOD | reconm. |', '|' + '---|' * 16]
    for r in rec_rows(log):
        s = r['s']; o = r['ok']
        out.append(f"| {s['vn']:g} V | {s['cl']*1e6:g} uF | {s['gap']*1e3:g} | {f(r['vds'])} | {f(r['vgs'])} | {f(r['vm'])} | {f(r['ipk'])} | {f(r['drop'])} | {r['n_pg']} | {r['n_re']} | {f(r['tf'])} | {f(None if r['e'] is None else r['e']*1e3)} | {f(r['u'])} | {'PASA' if o[0] else 'FALLA'} | {'PASA' if o[1] else 'FALLA'} | {'PASA' if o[2] else 'FALLA'} |")
    return '\n'.join(out)
