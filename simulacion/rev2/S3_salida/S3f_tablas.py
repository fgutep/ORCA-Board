"""S3f: genera los bloques markdown de resumen.md seccion 10 desde los .log (solo .meas).
Uso: python S3f_tablas.py niveles apertura_resumen rejilla recierre ilim soa25 precarga_c precarga_d"""
import sys
import s3_post as p
import S3f_post as q

f = q.f
SOA_KEYS = ('mxu100u', 'mxu1m', 'mxu10m', 'mxu40m', 'mxudc')


def util(m, i):
    g = lambda k: p._g(m, k, i)
    us = [g(k) for k in SOA_KEYS]
    e, pk = g('eq6'), g('ppk')
    teq = None if (e is None or not pk) else e / pk
    return (p.u_at(teq, us) if (None not in us and teq) else None), teq, e, pk


def niveles():
    out = ['| Rrst a source | V_GS Q6 a 6.8 V, 25 C [V] | idem 6.8 V, 100 C [V] | idem 12 V, 25 C [V] | idem 25 V, 25 C [V] | P(Q6) max [W] | P(Q6) a 6.8 V, 100 C [W] | abs(V_GS) >= 5 V en todo | P <= 0.5 W |', '|' + '---|' * 9]
    rows = []
    for log in ('S3f_niveles.log', 'S3f_niveles_ext.log'):
        st, m = p.parse_log(log)
        for i, s in enumerate(st):
            rows.append((s['rst'], s['vn'], s['temp'], m['vgs6'][i], m['p6'][i]))
    for R in sorted(set(r[0] for r in rows)):
        d = {(v, t): (g, pp) for r, v, t, g, pp in rows if r == R}
        gv = lambda v, t: d.get((v, t), (None, None))[0]
        allg = [abs(g) for g, _ in d.values()]
        Pm = max(pp for _, pp in d.values())
        p68 = d[(6.8, 100.0)][1]
        out.append(f"| {q.eng(R)} | {f(gv(6.8,25.0))} | {f(gv(6.8,100.0))} | {f(gv(12.0,25.0))} | {f(gv(25.0,25.0))} | {f(Pm)} | {f(p68)} | {'PASA' if min(allg) >= 5 else 'FALLA'} | {'PASA' if Pm <= 0.5 else 'FALLA'} |")
    return '\n'.join(out)


def apertura_resumen():
    out = ['| Rrst | V_OUT | L | t apagado I<10% [ms] | V_DS max sin D [V] | VOUT_EXT min sin D [V] | util SOA sin D | V_DS max con Schottky [V] | util SOA con D | I pico D [A] |', '|' + '---|' * 10]

    def load(log):
        st, m = p.parse_log(log)
        d = {}
        for i, s in enumerate(st):
            g = lambda k: p._g(m, k, i)
            u = util(m, i)[0]
            d[(s['rst'], s['vn'], s['ll'])] = dict(t10=None if g('toff10') is None else (g('toff10') - 2e-3) * 1e3, v=g('vdsmax'), ve=g('vextmin'), u=u, idpk=g('idpk'))
        return d
    a, b = load('S3f_apertura.log'), load('S3f_apertura_dfw.log')
    for (R, vn, ll) in a:
        if vn != 25:
            continue
        x, y = a[(R, vn, ll)], b[(R, vn, ll)]
        Ls = f'{ll*1e6:g} uH' if ll < 1e-3 else f'{ll*1e3:g} mH'
        out.append(f"| {q.eng(R)} | {vn:g} V | {Ls} | {f(x['t10'])} | {f(x['v'])} | {f(x['ve'])} | {f(x['u'])} | {f(y['v'])} | {f(y['u'])} | {f(y['idpk'])} |")
    return '\n'.join(out)


def rejilla():
    out = ['| Rrst | combinaciones que pasan caida+PGOOD+reconm. (de 27) | caida max BWK 1 / 3 / 1/3 [%] | util SOA > 1 (BWK=1) | V_GS final a 7.2 V [V] | observacion |', '|---|---|---|---|---|---|']
    for R in ('10k', '47k', '100k', '330k'):
        rows = p.a_rows(f'S3f_rejilla_R{R}.log')
        npass = sum(all(p.verdict_a(r)) for r in rows)
        mx = [max(r['drop'] for r in rows if abs(r['s']['bwk'] - b) < 1e-3) for b in (1, 3, 0.33333)]
        soa = ', '.join(f"{r['s']['vn']:g} V/{r['s']['cl']*1e6:g} uF (u={f(r['u'])})" for r in rows if abs(r['s']['bwk'] - 1) < 1e-3 and (r['u'] or 0) > 1)
        r72 = [r for r in rows if r['s']['vn'] == 7.2 and abs(r['s']['bwk'] - 1) < 1e-3][0]
        obs = ''
        if R == '10k':
            obs = 'FALSO PASA a 7.2 V: Q6 no cierra (I pico 0.79 A, nunca V_DS < 0.5 V); V_GS a 12 V = -3.85 V'
        elif R == '47k':
            obs = 'V_GS a 7.2 V < 5 V'
        out.append(f"| {R.replace('k', ' k')} | {npass}/27 | {f(mx[0])} / {f(mx[1])} / {f(mx[2])} | {soa} | {f(r72['vgsfin'])} | {obs} |")
    return '\n'.join(out)


def recierre():
    out = ['| Rrst | GAP [ms] | pasan las 3 (de 6) | fallan | I pico max [A] | caida max [%] | util SOA > 1 |', '|---|---|---|---|---|---|---|']
    for R in ('1G', '10k', '47k', '100k', '330k'):
        log = f'S3f_recierre_R{R}.log'
        rows = q.rec_rows(log)
        lab = 'ninguna' if R == '1G' else R.replace('k', ' k')
        for gap in (1e-3, 5e-3, 20e-3):
            rs = [r for r in rows if abs(r['s']['gap'] - gap) < 1e-9]
            bad = ', '.join(f"{r['s']['vn']:g} V/{r['s']['cl']*1e6:g} uF" for r in rs if not all(r['ok'])) or '-'
            soa = ', '.join(f"{r['s']['vn']:g} V/{r['s']['cl']*1e6:g} uF ({f(r['u'])})" for r in rs if (r['u'] or 0) > 1) or '-'
            out.append(f"| {lab} | {gap*1e3:g} | {sum(all(r['ok']) for r in rs)}/6 | {bad} | {f(max(r['ipk'] for r in rs))} | {f(max(r['drop'] for r in rs))} | {soa} |")
    return '\n'.join(out)


def ilim():
    names = [('nuevo', 'Rd', 'red nueva (diodo, Rrst 330 k), con Rd'), ('nuevo', 'sinRd', 'red nueva, SIN Rd'),
             ('viejo', 'Rd', 'red vieja (C gd directo = S3a_final), con Rd'), ('viejo', 'sinRd', 'red vieja, SIN Rd')]
    data = {}
    for net, rd, _ in names:
        for r in p.a_rows(f'S3f_ilim_{net}_{rd}.log'):
            data[(net, rd, r['s']['vn'], r['s']['cl'], r['s']['ilim'])] = r
    out = ['| V_OUT | C_carga | ' + ' | '.join(n[2] for n in names) + ' | ref. red nueva con Rd, ILIM 15 A |', '|' + '---|' * 7]
    for cl in (100e-6, 470e-6, 1000e-6):
        for vn in (7.2, 12.0, 25.0):
            cells = []
            for net, rd, _ in names:
                r = data[(net, rd, vn, cl, 10.0)]
                ok = p.verdict_a(r)
                cells.append(f"{'PASA' if all(ok) else 'FALLA'}: caida {f(r['drop'])} %, Ipk {f(r['ipk'])} A, PGOOD cae {r['n_pgf']}, reconm. {r['n_reop']}/{r['n_recl']}, u {f(r['u'])}")
            r15 = data[('nuevo', 'Rd', vn, cl, 15.0)]
            out.append(f"| {vn:g} V | {cl*1e6:g} uF | " + ' | '.join(cells) + f" | caida {f(r15['drop'])} %, Ipk {f(r15['ipk'])} A, u {f(r15['u'])} |")
    return '\n'.join(out)


def soa25():
    st, m = p.parse_log('S3f_soa25.log')
    out = ['| C_carga [uF] | I pico Q6 [A] | caida [%] | t encendido (V_DS<0.5 V) [ms] | t con V_DS > 15 V [ms] | P pico [W] | E [mJ] | t_eq [ms] | util SOA(t_eq) | veredicto SOA |', '|' + '---|' * 10]
    for i, s in enumerate(st):
        g = lambda k: p._g(m, k, i)
        u, teq, e, pk = util(m, i)
        out.append(f"| {s['cl']*1e6:g} | {f(g('ipk'))} | {f(g('drop'))} | {f((g('tfull')-1e-3)*1e3)} | {f(g('t15')*1e3)} | {f(pk)} | {f(e*1e3)} | {f(teq*1e3)} | {f(u)} | {p.soa_flag(u)} |")
    return '\n'.join(out)


def precarga(log):
    st, m = p.parse_log(log)
    out = ['| C_carga [uF] | Rpre | t de precarga antes de cerrar [ms] | V_ext al cerrar [V] | V_DS al cerrar [V] | P(Rpre) pico [W] | I pico Q6 [A] | t con V_DS > 15 V [ms] | util SOA(t_eq) | veredicto SOA |', '|' + '---|' * 10]
    for i, s in enumerate(st):
        g = lambda k: p._g(m, k, i)
        u = util(m, i)[0]
        rp = 'ninguna' if s['rpre'] >= 1e9 else f"{s['rpre']:g} ohm"
        out.append(f"| {s['cl']*1e6:g} | {rp} | {s['tclose']*1e3:g} | {f(g('vextc'))} | {f(g('vdsc'))} | {f(g('ppre'))} | {f(g('ipk'))} | {f(g('t15')*1e3)} | {f(u)} | {p.soa_flag(u)} |")
    return '\n'.join(out)


def recierre_largo():
    out = ['| V_OUT | C_carga | GAP [ms] | V(m)-V(vint) al recierre [V] | I pico [A] | caida [%] | PGOOD cae (n) | reaperturas (n) | util SOA | caida / PGOOD / reconm. |', '|' + '---|' * 10]
    for r in q.rec_rows('S3f_recierre_R330k_largo.log'):
        s = r['s']; o = r['ok']
        out.append(f"| {s['vn']:g} V | {s['cl']*1e6:g} uF | {s['gap']*1e3:g} | {f(r['vm'])} | {f(r['ipk'])} | {f(r['drop'])} | {r['n_pg']} | {r['n_re']} | {f(r['u'])} | {'PASA' if o[0] else 'FALLA'} / {'PASA' if o[1] else 'FALLA'} / {'PASA' if o[2] else 'FALLA'} |")
    return '\n'.join(out)


if __name__ == '__main__':
    for n in sys.argv[1:]:
        r = precarga('S3f_precarga_conectada.log') if n == 'precarga_c' else precarga('S3f_precarga_difer.log') if n == 'precarga_d' else globals()[n]()
        print('###', n)
        print(r)
