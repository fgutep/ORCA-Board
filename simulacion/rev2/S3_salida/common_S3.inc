* common_S3.inc - etapa de salida Rev2 (Q6, Q7, PGOOD, LEDs, bleeder) + convertidor idealizado
* Modelos de fabricante (ruta relativa, sin copiar)
.include "..\modelos\SiRS4401DP_PS.TXT"
.include "..\modelos\2n7002k_ps Rev B.lib"
* Modelos de libreria LTspice (standard.dio, copia literal). NO son del MM3Z12VT1G, del LED real ni del B5819W.
.model BZX84C12L D(Is=.6n Rs=.5 Cjo=150p nbv=5 bv=12 Ibv=1m Vpk=12 mfg=OnSemi type=Zener)
.model QTLP690C D(Is=1e-22 Rs=6 N=1.5 Cjo=50p Xti=100 Iave=160m Vpk=5 mfg=Fairchild type=LED)
.model MBR745 D(Is=159u Rs=.02 N=1.76 Cjo=740p M=.6 Eg=.69 Xti=2 Iave=7.5 Vpk=45 mfg=OnSemi type=Schottky)
.model SWM SW(Ron=1m Roff=1G Vt=0.5 Vh=0)
* ---------------- parametros ----------------
.param VN=12 BWK=1 ILIM=15 KCL=20 ESRL=10m
.param RFB={49.9k*0.8/(VN-0.8)}
* remedios: CGD (C gate-drain Q6), RG (R de gate Q6), RQ7/CQ7 (RC en gate de Q7) se definen en cada caso (base: CGD=1f RG=10k RQ7=1m CQ7=1p)
* TSRCOFF (instante en que se desconecta la fuente) se define en cada caso
* ---------------- convertidor idealizado ----------------
* fuente VN -> limitador de corriente (resistencia serie KCL cuando |I|>ILIM) -> [Ls 4.7u*BWK^2, sin DCR: Zout(DC)=0] // Rd=0.1*BWK
* f0 = 1/(2pi*sqrt(L*Cout)) = 3.4 kHz con BWK=1 ; zeta = sqrt(L/C)/(2*Rd) = 0.5 ; Zout(DC)=0 (como un lazo con integrador)
Bsrc nb 0 V={VN}-{KCL}*(max(I(Vsns)-{ILIM},0)+min(I(Vsns)+{ILIM},0))
Vsns nb nc 0
Ssrc nc nm csrc 0 SWM
Vcsrc csrc 0 PULSE(1 0 {TSRCOFF} 1u 1u 1e6 2e6)
Ls nm vint {4.7u*BWK*BWK}
Rd nm vint {0.1*BWK}
* ---------------- C_OUT en VOUT_INT ----------------
Co1 vint c1 440u
Re1 c1 0 12m
Co2a vint c2a 10u
Re2a c2a 0 3m
Co2b vint c2b 10u
Re2b c2b 0 3m
Co3 vint c3 100n
Re3 c3 0 10m
* ---------------- bleeder y FB ----------------
Rbleed vint 0 1.5k
Rfbt vint fb 49.9k
Rfbb fb 0 {RFB}
* ---------------- PGOOD: comparador de ventana de comportamiento, open-drain Ron=100, sin deglitch ----------------
* ventana FB en [0.728, 0.880] V ; histeresis 1.6 % de 0.8 V = 12.8 mV por flanco
Vcc vcc 0 7.35
Rpg vcc pgood 100k
Supg pgood 0 0 fb UVM
Sopg pgood 0 fb 0 OVM
.model UVM SW(Ron=100 Roff=1G Vt=-0.7344 Vh=0.0064)
.model OVM SW(Ron=100 Roff=1G Vt=0.8736 Vh=0.0064)
* ---------------- Q7 (gate = PGOOD, opcional RC de retardo) y LED2 ----------------
Rq7 pgood gq7 {RQ7}
Cq7 gq7 0 {CQ7}
XQ7 pgd gq7 0 2N7002K
Rl2 vint l2a 4.7k
DL2 l2a pgd QTLP690C
* ---------------- Q6 y su gate ----------------
XQ6 vdr g6 vint SiRS4401DP
Vid vdr vext 0
Rgs g6 vint 100k
DZ6 g6 vint BZX84C12L
Rg6 g6 sw1a {RG}
S1 sw1a pgd ctl 0 SWM
Cgd6 g6 vdr {CGD}
* LED3 en VOUT_EXT
Rl3 vext l3a 4.7k
DL3 l3a 0 QTLP690C
