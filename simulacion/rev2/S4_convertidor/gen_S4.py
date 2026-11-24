"""Genera los .cir de S4b-e a partir de UNA plantilla (todas las corridas comparten netlist; solo cambian .param y bloques .meas).
Uso: python gen_S4.py   (reescribe S4b.cir, S4cd_*.cir, S4c_sin.cir, S4e_*.cir)"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# valle de I(L1): minimo deslizante sobre 7 us (> periodo ~5.7 us con dither), 28 tomas a 0.25 us
vy = "I(L1)"
for k in range(1, 28):
    vy = f"min({vy},delay(I(L1),{k * 0.25:g}u))"

BASE = r"""@TITLE@
* Base: S4a.cir (LM5175 + Q1-Q4 BUK9Y3R5-40E + L + caps). Agregados Rev2: entrada Q5 + salida Q6/Q7 + bleeder. ISNS (N010 N010) igual que S4a.
XU1 N002 N004 VIN N005 N006 N008 SLOPE SS COMP 0 FB VOUT N010 N010 CSG CS PGOOD SW2 HDRV2 N007 LDRV2 0 VCC VOUT LDRV1 N009 HDRV1 SW1 NC_02 LM5175_TRANS SS_1=0
R1 N008 0 71.5k
C1 SLOPE 0 270p
C2 SS 0 {CSS}
R2 COMP N011 2.43k
C3 COMP 0 470p
C4 N011 0 27n
C5 N006 0 39n
R3 0 FB {RFB_BOT}
R4 FB VOUT {RFB_TOP}
C6 0 VCC 1u
R5 VIN N002 200k
R6 N002 0 66.5k
* Q1-Q4 = BUK9Y3R5-40E (DRAIN GATE SOURCE)
XU2 VIN HDRV1 SW1 BUK9Y3R5-40E
XU3 SW2 LDRV2 N001 BUK9Y3R5-40E
XU4 VOUT HDRV2 SW2 BUK9Y3R5-40E
XU5 SW1 LDRV1 N001 BUK9Y3R5-40E
L1 SW1 SW2 4.7u Ipk=35 Rser=4.8m
C7 VIN 0 100u Rser=30m
R8 N001 0 5m
R9 N001 CS 100
R10 0 CSG 100
C9 CS CSG 100p
D1 VIN N003 1N5819
R11 N004 N003 10
C10 N004 0 0.1u
D2 VCC N009 1N5819
C11 N009 SW1 0.1u
D3 VCC N007 1N5819
C12 N007 SW2 0.1u
R12 N005 0 93.1k
C8 VOUT 0 440u Rser=12m
C13 VOUT 0 10u Rser=3m
C14 VOUT 0 10u Rser=3m
C15 VOUT 0 100n Rser=10m
C16 VIN 0 10u Rser=3m
C17 VIN 0 100n Rser=10m
RPG VCC PGOOD 100k
* carga resistiva en VOUT (S4b: 8 A como S4a; S4c-e: 1G, la carga va en VEXT detras de Q6)
R7 VOUT 0 {R7_val}
* bleeder 1.5k en VOUT
Rbleed VOUT 0 1.5k
* ---------------- entrada de proteccion: bateria -> R_bat -> L_cable -> F1 -> Q5 -> VIN ----------------
* rampa de bateria 20 us (README: 200 us). Con Q5 en el camino, rampas >= 100 us dejan la gate de Q5 'pegada' a VIN (V_GS ~ -0.1 V, Q5 apagado por su diodo de cuerpo, Vin ~ 11.1 V) y con 200 us el solver se atasca a 0.2227 ms; con 5 y 20 us Q5 enciende. Las 8 A salen de VIN a traves de Q5.
V1 NBAT 0 PWL(0 0 20u {VIN_dc})
Rbat NBAT NB1 20m
Lcab NB1 NB2 0.5u
Rf1 NB2 NB3 1.92m
Vid5 NB3 DQ5 0
XQ5 DQ5 G5 VIN SiRS4401DP
Rg5 G5 0 47k
DZ5 G5 VIN BZ12L1P
* ---------------- salida: Q6 (P-ch, source en VOUT) + Q7 (gate = PGOOD) ----------------
XQ7 PGD PGOOD 0 2N7002K
XQ6 VDR G6 VOUT SiRS4401DP
Vid VDR VEXT 0
Rgs G6 VOUT 100k
DZ6 G6 VOUT BZ12L1P
Rg6 G6 SW1A {RG}
S1 SW1A PGD CTL 0 SWM
@REMEDIO@
Vctl CTL 0 PWL(0 0 {TCLOSE} 0 {TCLOSE+1n} 1 {TOPEN} 1 {TOPEN+1n} 0)
Cl VEXT CL1 {CL}
Rcl CL1 0 10m
Rload VEXT 0 {RLOADX}
* ---------------- filtros de medida ----------------
* mediana de 3 muestras a 25 ns (ventana ~50 ns) sobre V(vout): rechaza picos numericos < 25 ns
Bvm vmed 0 V=max(min(V(vout),delay(V(vout),25n)),min(max(V(vout),delay(V(vout),25n)),delay(V(vout),50n)))
* valle de I(L1): minimo deslizante de 7 us (28 tomas a 0.25 us); resolucion ~ 0.25 us x di/dt (estimacion: <~0.3 A)
Bvyl vyl 0 V=@VY@
* ---------------- modelos ----------------
.lib C:\Users\Lenovo\AppData\Local\LTspice\lib\cmp\standard.dio
* BZX84C12L de standard.dio con Cjo=1p (desviacion documentada; la libreria trae 150p)
.model BZ12L1P D(Is=.6n Rs=.5 Cjo=1p nbv=5 bv=12 Ibv=1m Vpk=12 mfg=OnSemi type=Zener)
.model D1N4148W D(Is=2.52n Rs=.568 N=1.752 Cjo=4p M=.4 tt=20n Iave=200m Vpk=75 mfg=OnSemi type=silicon)
.model SWM SW(Ron=1m Roff=1G Vt=0.5 Vh=0)
.include ..\LM5175_TRANS_xorfix.lib
.include ..\modelos\BUK9Y3R5-40E.lib
.include ..\modelos\SiRS4401DP_PS.TXT
.include "..\modelos\2n7002k_ps Rev B.lib"
.ic V(ss)=0 V(comp)=0 V(vout)=0
.param CSS=12n
.param VIN_dc=@VIN@
.param VOUT_target=@VOUT@
.param RFB_TOP=49.9k
.param RFB_BOT={RFB_TOP/(VOUT_target/0.8-1)}
.param R7_val=@R7@
.param RLOADX=@RLOADX@
.param CL=@CL@
.param RG=@RG@
.param TCLOSE=@TCLOSE@
.param TOPEN=@TOPEN@
.param TEND=@TEND@
.tran 0 {TEND} 0 100n
.options solver=alt method=modtrap
.save V(vout) V(vext) V(fb) V(ss) V(comp) V(pgood) V(vcc) I(L1) V(vin) I(Vid5) I(Vid) V(g6) V(g5) V(dq5) V(vmed) V(vyl)
@MEAS@
.end
"""

REM_SI = ("* remedio Q6: C_gd 100 nF 0805 en serie con 1N4148W, reposicion Rrst 330k a source, R_G 22k\n"
          "Cgd6 VDR M 100n\nDgd M G6 D1N4148W\nRrst M VOUT 330k")
REM_NO = "* sin remedio: R_G 10k (baseline del circuito original), sin C_gd/diodo/Rrst"

MEAS_REG = r"""* ---- S4b: regimen (misma ventana que S4a) ----
.meas TRAN il_max MAX I(L1)
.meas TRAN il_avg AVG I(L1) FROM 2.4m TO 2.6m
.meas TRAN il_pk MAX I(L1) FROM 2.4m TO 2.6m
.meas TRAN il_vy MIN I(L1) FROM 2.4m TO 2.6m
.meas TRAN vout_reg AVG V(vout) FROM 2.4m TO 2.6m
.meas TRAN fb_reg AVG V(fb) FROM 2.4m TO 2.6m
.meas TRAN vout_pp PP V(vout) FROM 2.4m TO 2.6m
.meas TRAN vout_ppm PP V(vmed) FROM 2.4m TO 2.6m
.meas TRAN vcc_reg AVG V(vcc) FROM 2.4m TO 2.6m
.meas TRAN vin_reg AVG V(vin) FROM 2.4m TO 2.6m
.meas TRAN ibat_avg AVG I(Vid5) FROM 2.4m TO 2.6m
.meas TRAN il_avg_b AVG I(L1) FROM 2.6m TO 2.8m
.meas TRAN vout_reg_b AVG V(vout) FROM 2.6m TO 2.8m
.meas TRAN fb_reg_b AVG V(fb) FROM 2.6m TO 2.8m
.meas TRAN il_avg_c AVG I(L1) FROM 2.8m TO 3.0m
.meas TRAN vout_reg_c AVG V(vout) FROM 2.8m TO 3.0m
.meas TRAN fb_reg_c AVG V(fb) FROM 2.8m TO 3.0m
.meas TRAN vyl_max_reg MAX V(vyl) FROM 2.4m TO 2.6m
.meas TRAN t_pgood WHEN V(pgood)=3.5 RISE=1
.meas TRAN t_vout11 WHEN V(vout)=11 RISE=1
.meas TRAN vss_end FIND V(ss) AT 3.0m
.meas TRAN vcomp_end FIND V(comp) AT 3.0m
.meas TRAN vgs5_reg AVG V(g5)-V(vin) FROM 2.4m TO 2.6m
.meas TRAN vds5_reg AVG V(dq5)-V(vin) FROM 2.4m TO 2.6m
"""

MEAS_CLOSE = r"""* ---- cierre de Q6 en TCLOSE (valor previo = promedio de los 100 us anteriores) ----
.meas TRAN vpre_c AVG V(vout) FROM {TCLOSE-0.1m} TO {TCLOSE}
.meas TRAN vppm_pre_c PP V(vmed) FROM {TCLOSE-0.1m} TO {TCLOSE}
.meas TRAN vmin_c MIN V(vmed) FROM {TCLOSE} TO {TCEND}
.meas TRAN drop_c PARAM (vpre_c-vmin_c)/vpre_c*100
.meas TRAN vmin_c_raw MIN V(vout) FROM {TCLOSE} TO {TCEND}
.meas TRAN vpost_c AVG V(vout) FROM {TCEND-0.2m} TO {TCEND}
.meas TRAN ilpk_c MAX I(L1) FROM {TCLOSE} TO {TCEND}
.meas TRAN ilavg_pre_c AVG I(L1) FROM {TCLOSE-0.1m} TO {TCLOSE}
.meas TRAN vyl_pre_c MAX V(vyl) FROM {TCLOSE-0.3m} TO {TCLOSE}
.meas TRAN vyl_max_c MAX V(vyl) FROM {TCLOSE} TO {TCEND}
.meas TRAN vyl_max_c_post MAX V(vyl) FROM {TCLOSE+0.01m} TO {TCEND}
.meas TRAN ipk_q6 MAX I(Vid) FROM {TCLOSE} TO {TCEND}
.meas TRAN t_q6on WHEN V(vout)-V(vext)=0.5 FALL=1 TD={TCLOSE}
.meas TRAN pgf_c1 WHEN V(pgood)=3.5 FALL=1 TD={TCLOSE}
.meas TRAN pgf_c2 WHEN V(pgood)=3.5 FALL=2 TD={TCLOSE}
.meas TRAN pgmin_c MIN V(pgood) FROM {TCLOSE} TO {TCEND}
.meas TRAN gon_c1 WHEN V(g6)-V(vout)=-2.5 FALL=1 TD={TCLOSE}
.meas TRAN goff_c1 WHEN V(g6)-V(vout)=-2.5 RISE=1 TD={TCLOSE}
.meas TRAN gon_c2 WHEN V(g6)-V(vout)=-2.5 FALL=2 TD={TCLOSE}
.meas TRAN ssmin_c MIN V(ss) FROM {TCLOSE} TO {TCEND}
.meas TRAN ss_pre_c FIND V(ss) AT {TCLOSE}
.meas TRAN vcomp_max_c MAX V(comp) FROM {TCLOSE} TO {TCEND}
.meas TRAN vgs6_post AVG V(g6)-V(vout) FROM {TCEND-0.2m} TO {TCEND}
.meas TRAN vext_post AVG V(vext) FROM {TCEND-0.2m} TO {TCEND}
"""

MEAS_OPEN = r"""* ---- apertura de Q6 en TOPEN (valor previo = promedio de los 200 us anteriores) ----
.meas TRAN vpre_o AVG V(vout) FROM {TOPEN-0.2m} TO {TOPEN}
.meas TRAN vppm_pre_o PP V(vmed) FROM {TOPEN-0.2m} TO {TOPEN}
.meas TRAN vmax_o MAX V(vmed) FROM {TOPEN} TO {TEND}
.meas TRAN os_o PARAM (vmax_o-vpre_o)/vpre_o*100
.meas TRAN vmax_o_raw MAX V(vout) FROM {TOPEN} TO {TEND}
.meas TRAN vfin_o AVG V(vout) FROM {TEND-0.2m} TO {TEND}
.meas TRAN ilavg_pre_o AVG I(L1) FROM {TOPEN-0.2m} TO {TOPEN}
.meas TRAN ilpk_o MAX I(L1) FROM {TOPEN} TO {TEND}
.meas TRAN ilmin_o MIN I(L1) FROM {TOPEN} TO {TEND}
.meas TRAN t_q6off WHEN I(Vid)=0.8 FALL=1 TD={TOPEN}
.meas TRAN goff_o1 WHEN V(g6)-V(vout)=-2.5 RISE=1 TD={TOPEN}
.meas TRAN vext_min_o MIN V(vext) FROM {TOPEN} TO {TEND}
.meas TRAN pgf_o1 WHEN V(pgood)=3.5 FALL=1 TD={TOPEN}
.meas TRAN pgmin_o MIN V(pgood) FROM {TOPEN} TO {TEND}
.meas TRAN ssmin_o MIN V(ss) FROM {TOPEN} TO {TEND}
.meas TRAN vgs6_pre_o AVG V(g6)-V(vout) FROM {TOPEN-0.2m} TO {TOPEN}
.meas TRAN vext_pre_o AVG V(vext) FROM {TOPEN-0.2m} TO {TOPEN}
"""


def case(name, title, vin, vout, remedio, close=None, open_=None, tend=3e-3, reg=False):
    rload = f"{vout / 8:g}"
    t = BASE
    meas = ""
    if reg:
        meas += MEAS_REG
    if close is not None:
        tcend = open_ if open_ is not None else tend
        meas += f".param TCEND={tcend:g}\n" + MEAS_CLOSE
    if open_ is not None:
        meas += MEAS_OPEN
    rep = {
        "@TITLE@": title, "@REMEDIO@": REM_SI if remedio else REM_NO, "@VY@": vy,
        "@VIN@": f"{vin:g}", "@VOUT@": f"{vout:g}",
        "@R7@": "1.5" if reg else f"{vout:g}", "@RLOADX@": "1G" if reg else rload, "@CL@": "1p" if reg else "470u",
        "@RG@": "22k" if remedio else "10k",
        "@TCLOSE@": "1" if close is None else f"{close:g}",
        "@TOPEN@": "2" if open_ is None else f"{open_:g}",
        "@TEND@": f"{tend:g}", "@MEAS@": meas,
    }
    for k, v in rep.items():
        t = t.replace(k, v)
    with open(os.path.join(HERE, name + ".cir"), "w", newline="\n") as f:
        f.write(t)


if __name__ == "__main__":
    case("S4b", "S4b convertidor + Q5 + R_bat + L_cable + bleeder + PGOOD, Q6 abierto, 12V->12V 8A en VOUT (CSS=12n)",
         12, 12, True, reg=True)
    case("S4cd_con", "S4c+S4d 12V->12V: Q6 cierra a 3 ms contra 8A+470uF, abre a 8.5 ms; CON remedio (Cgd 100n+1N4148W+Rrst 330k, RG 22k)",
         12, 12, True, close=3e-3, open_=8.5e-3, tend=17e-3)
    case("S4c_sin", "S4c 12V->12V: Q6 cierra a 3 ms contra 8A+470uF; SIN remedio (RG 10k)",
         12, 12, False, close=3e-3, tend=6e-3)
    case("S4e_7v4_10v", "S4e 7.4V->10V: cierre 3 ms (solo cierre, 7 ms), con remedio", 7.4, 10, True, close=3e-3, tend=7e-3)
    case("S4e_16v8_7v2", "S4e 16.8V->7.2V: cierre 3 ms (solo cierre, 7 ms), con remedio", 16.8, 7.2, True, close=3e-3, tend=7e-3)
    case("S4e_22v2_24v", "S4e 22.2V->24V: cierre 3 ms (solo cierre, 7 ms), con remedio", 22.2, 24, True, close=3e-3, tend=7e-3)
    print("ok")



