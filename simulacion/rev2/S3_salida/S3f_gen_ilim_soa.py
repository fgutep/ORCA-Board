base = open('S3a_final.cir', encoding='utf-8').read().replace('include common_S3.inc', 'include S3f_common.inc')
head_old = 'S3a FINAL: cierre de SW1 con remedio elegido (C gd 100n + RG 22k, COMBINACION rotulada R1+R2); sensibilidad BWK 1 / 3 / 1/3'
P_old = 'CGD=100n RG=22k RQ7=1m CQ7=1p'
# ---- Tarea 2: ILIM 10 A con/sin Rd, red nueva (330k) y red vieja (Cgd directo = RSTG 1m) ----
for net, rst, rstg in (('nuevo', '330k', '1G'), ('viejo', '1G', '1m')):
    for rd in (1, 0):
        t = base.replace(head_old, f'S3f ILIM 10/15 A, Rd {"con" if rd else "SIN"}; red {net} (RST={rst} RSTG={rstg}); VN x CL x ILIM, BWK=1')
        t = t.replace(P_old, f'{P_old} RST={rst} RSTG={rstg} RDON={rd} RPRE=1G')
        t = t.replace('.step param BWK list 1 3 0.33333', '.step param ILIM list 10 15')
        assert 'ILIM list' in t and f'RDON={rd}' in t
        open(f'S3f_ilim_{net}_{"Rd" if rd else "sinRd"}.cir', 'w', encoding='utf-8').write(t)
# ---- Tarea 3a: SOA 25 V vs C_carga ----
t = base.replace(head_old, 'S3f SOA del encendido a 25 V vs C_carga (red nueva RST=330k), tiempo con V_DS>15 V')
t = t.replace(P_old, f'{P_old} RST=330k RSTG=1G RDON=1 RPRE=1G')
t = t.replace('.step param VN list 7.2 12 25\n.step param CL list 100u 470u 1000u\n.step param BWK list 1 3 0.33333', '.step param CL list 47u 68u 100u 150u 180u 220u 270u 330u 470u')
t = t.replace('.param CL=470u', '.param VN=25 CL=470u')  # VN fijo: LTspice rechaza un .step de un solo valor
t = t.replace('.meas tran fbmin', '.meas tran t15 integ (V(vint)-V(vdr)>15) from={TCLOSE}\n.meas tran t15b when V(vint)-V(vdr)=15 fall=1 td={TCLOSE}\n.meas tran fbmin')
assert 't15' in t and 'list 47u' in t
open('S3f_soa25.cir', 'w', encoding='utf-8').write(t)
