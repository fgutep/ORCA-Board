# S3 - Interruptor de salida Q6, PGOOD/Q7, LEDs y bleeder (Rev. 2)

LTspice 26.0.2, convertidor idealizado (sin LM5175). Los numeros de las tablas salen de `.meas` de los `.log` de esta carpeta; el unico post-proceso es el calculo de veredictos, la interpolacion del pulso equivalente SOA (`s3_post.py`) y los CSV. Lo que no es `.meas` va rotulado **(inferencia)**, **(supuesto)** o **(artefacto)** donde aparece.

**Regla de margen para V_DS (unica, valida en §2, §4, §8 y §10; la misma que S1/S2)**: limite de diseno 32 V; AL LIMITE si V_DS > 25.6 V (margen < 20 %, 0.8 x 32 V); FALLA si V_DS >= 32 V. Con ella, 30.7 V (25 V, 1 mH, sin diodo) es **AL LIMITE** (margen 1.3 V = 4 % de 32 V) en todas las secciones; 25.5 V con Schottky es PASA, justo bajo el umbral (0.1 V).

## 1. Montaje (resumen)

- Q6 = SiRS4401DP (modelo Vishay `..\modelos\SiRS4401DP_PS.TXT`), Q7 = 2N7002K (modelo Vishay `..\modelos\2n7002k_ps Rev B.lib`), incluidos por ruta relativa. Resto segun el enunciado (100 k G-S, zener G-S, 10 k a SW1, LED2/LED3 con 4.7 k, bleeder 1.5 k, C_OUT 440 uF/12 mohm + 2x10 uF/3 mohm + 100 nF/10 mohm, FB 49.9 k / R_bot con 0.8 V al nominal).
- Modelos de `standard.dio` (NO son las piezas reales; el zener lleva una desviacion declarada, ver abajo):
  - `.model BZX84C12L D(Is=.6n Rs=.5 Cjo=1p nbv=5 bv=12 Ibv=1m Vpk=12 mfg=OnSemi type=Zener)` (zener 12 V). **Decision de proyecto: `Cjo=1p`**, desviacion de la libreria, que trae `Cjo=150p` (igual que S1/S2/S4b-e). **Aviso (pendiente de verificar)**: `common_S3.inc` y `S3f_common.inc` (linea 6) todavia tienen `Cjo=150p`, es decir los `.log` de este banco se corrieron con 150p y no se re-simularon con 1p; el efecto en los resultados de S3 no esta medido.
  - `.model QTLP690C D(Is=1e-22 Rs=6 N=1.5 Cjo=50p Xti=100 Iave=160m Vpk=5 mfg=Fairchild type=LED)` (LED1/2/3)
  - `.model MBR745 D(Is=159u Rs=.02 N=1.76 Cjo=740p M=.6 Eg=.69 Xti=2 Iave=7.5 Vpk=45 mfg=OnSemi type=Schottky)` (rueda libre; el 1N5819 de la BOM es de 1 A y no sirve para 8 A)
- **Convertidor idealizado**: fuente V_OUT -> limitador de corriente (resistencia serie de 20 ohm cuando |I| > 15 A; pico medido 15.0-15.3 A) -> L_s = 4.7 uH x BWK^2 en paralelo con R_d = 0.1 ohm x BWK. Justificacion: con C_OUT = 460 uF, f0 = 1/(2 pi sqrt(L C)) = 3.4 kHz (BWK=1, ~ f_sw/100) y zeta = sqrt(L/C)/(2 R_d) = 0.5. El amortiguador en paralelo con L (en vez de R serie) da impedancia de salida 0 en continua, como un lazo con integrador, y R_d en alta frecuencia (un escalon de carga hunde ~ dI x 0.1 ohm, igual a 1/(2 pi fc C) = 0.1 ohm). Sensibilidad: BWK = 3 (fuente 3x mas lenta: L x9, R_d x3) y BWK = 1/3 (3x mas rapida).
- **PGOOD**: comparador de ventana de comportamiento (dos interruptores con histeresis): cae si FB < 0.728 V o FB > 0.880 V; recupera a 0.7408 V / 0.8672 V (histeresis 1.6 % de 0.8 V = 12.8 mV); open-drain Ron = 100 ohm; pull-up 100 k a 7.35 V. **Sin deglitch (peor caso).**
- **Carga de S3a**: C_carga (ESR 10 mohm, supuesto) descargada en t = 0, mas R que toma 8 A al V_OUT nominal (R = V/8); SW1 cierra en t = 1 ms.
- **SOA**: curvas de la pagina 4 del datasheet (TC = 25 C, pulso unico) digitalizadas por color (+-10 %) e incrustadas como tablas en `soa_S3.inc`; `.meas max` de la *utilizacion* I/I_SOA(V_DS) para las curvas 100 us / 1 ms / 10 ms / 40 ms / DC (solo con V_DS > 0.5 V). Se interpola en log t al **pulso equivalente t_eq = E/P_pico** (E y P_pico medidos con `.meas`). Utilizacion <= 0.7 PASA, 0.7-1.0 AL LIMITE, > 1 FALLA.

## 2. Tabla resumen

| Caso | Criterio | Medido | Veredicto |
|---|---|---|---|
| S3a base (sin remedio) | caida VOUT_INT <= 5 % | 5.6-20.7 % (BWK=1: 9.0-20.5 %); 0/9 combinaciones cumplen | FALLA |
| S3a base | PGOOD no cae | cae en 9/9 (BWK=1; 8/9 con BWK=1/3); con chatteo, >= 5 caidas | FALLA |
| S3a base | Q6 no reconmuta | reapertura/recierre en 4/9 (12 V/470 uF, 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF: 1 a >= 5 ciclos); con BWK=3 en 9/9 | FALLA |
| S3a base | I pico Q6 / SOA | 19.8-146 A; SOA: 25 V/1000 uF u=1.19, 25 V/470 uF u=0.96 | FALLA / AL LIMITE |
| S3a final (C gd 100 nF + RG 22 k) | caida <= 5 %, PGOOD, sin reconmutacion | 9/9: caida max 2.19 % (BWK=1), 4.39 % (BWK=3), 2.16 % (BWK=1/3); PGOOD no cae; 0 reconmutaciones | PASA |
| S3a final | I pico Q6 | 7.97-15.4 A (limite de la fuente 15 A) | PASA |
| S3a final | SOA (utilizacion) | 25 V/470 uF u=1.79; 25 V/1000 uF u=3.14; resto <= 0.58; a 25 V: 150 uF u=0.72, 220 uF u=0.96, 330 uF u=1.37 | FALLA a 25 V con C >= 330 uF; AL LIMITE con 150-220 uF; PASA <= 100 uF |
| S3b sin diodo, sin remedio | V_DS < 32 V | max 30.73 V (25 V, 1 mH; > 25.6 V); pico negativo VOUT_EXT -9.78 V (12 V, 1 mH) | AL LIMITE (25 V, 1 mH); resto PASA |
| S3b con Schottky | V_DS < 32 V | max 25.5 V (< 25.6 V); VOUT_EXT min -0.53 V | PASA (justo bajo el umbral) |
| S3b tiempo de apagado (I < 10 %) | informativo | 7.3-8.2 ms sin remedio; 34.5 / 57.5 ms (12 / 25 V) con C gd 47 nF; 65 ms (12 V) y > 100 ms (25 V) con C gd 100 nF | informativo |
| S3b con remedio, 25 V, 8 A | SOA del apagado | u = 1.88 con 100 nF (1.53 con 47 nF); sin remedio u = 0.72 | FALLA |
| S3c regimen, sin remedio | 5 <= abs(V_GS Q6) <= 15 V | 6.12 V (6.8 V), 10.8-10.9 V (12 V), 12.0 V (25 V); V_GS Q7 = 7.35 V | PASA |
| S3c | P(Q6) <= 0.5 W a 8 A | 0.107-0.166 W (25 y 100 C) | PASA |
| S3c con remedio (RG 22 k) | idem | abs(V_GS Q6) = 5.47 V (6.8 V, 100 C) a 11.9 V; P <= 0.172 W | PASA (margen 0.47 V a 6.8 V) |
| S3d bleeder | 25 -> 5 V en ~1.1 s; P ~ 0.42 W a 25 V | 1.07 s; 0.417 W (0.552 W con FB y LED2) | PASA |
| S3d | 12 V | 5 V en 0.575 s, 2 V en 1.19 s; 0.096 W | informativo |
| S3e C_IN | informativo | desde 12 V: 5 V en 0.58 s, 2 V en 1.67 s, **1 V en 14.8 s**; desde 25.2 V: 5 V en 1.0 s, 2 V en 2.09 s, 1 V en 15.3 s; la corriente del LED cae < 10 uA a 1.57 V (t = 2.84 / 3.26 s) y despues solo descarga el divisor EN (tau ~ 29 s) | informativo |

## 3. S3a - cierre de SW1 contra C_carga

### 3.1 Antes del remedio (BWK = 1) - `S3a_base.log`
Rango de la caida base: la tabla es BWK = 1 (9.0-20.5 %, pasos 10-18 del `.meas drop`); con las tres velocidades (pasos 1-27) es 5.61-20.69 %, leido de `S3a_base.log`. `ESTADO_PAUSA.md` citaba "9-20 %" (solo BWK = 1, redondeado).
Reapert./recierres = numero de reaperturas / recierres de Q6 (V_GS cruza -2.5 V); los contadores saturan en 5 (leer "5" como ">= 5"). `t encendido` = SW1 cerrado -> V_DS < 0.5 V. `PGOOD cae (n)` cuenta caidas de PGOOD (satura en 5).

| V_OUT | C_carga | I pico Q6 [A] | caida VOUT_INT [%] | PGOOD cae (n) | reapert./recierres Q6 | t encendido [ms] | P pico [W] | E Q6 [mJ] | t_eq [us] | util SOA(t_eq) | caida | PGOOD | reconm. | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 7.2 V | 100 uF | 19.8 | 9 | 1 | 0/0 | 0.202 | 92.3 | 15.3 | 166 | 0.0825 | FALLA | FALLA | PASA | PASA |
| 12 V | 100 uF | 46.9 | 9.12 | 2 | 0/0 | 0.131 | 347 | 17.9 | 51.7 | 0.135 | FALLA | FALLA | PASA | PASA |
| 25 V | 100 uF | 122 | 9.27 | 3 | 0/0 | 0.148 | 1.98e+03 | 39.7 | 20 | 0.644 | FALLA | FALLA | PASA | PASA |
| 7.2 V | 470 uF | 28.5 | 10.7 | 5 | 0/0 | 0.337 | 164 | 24.1 | 147 | 0.135 | FALLA | FALLA | PASA | PASA |
| 12 V | 470 uF | 58.9 | 12.8 | 5 | 1/1 | 0.462 | 570 | 44.1 | 77.4 | 0.187 | FALLA | FALLA | FALLA | PASA |
| 25 V | 470 uF | 143 | 16.9 | 5 | 5/5 | 0.908 | 2.91e+03 | 152 | 52.2 | 0.956 | FALLA | FALLA | FALLA | AL LIMITE |
| 7.2 V | 1000 uF | 29.5 | 11.5 | 5 | 0/0 | 0.61 | 178 | 37.7 | 212 | 0.166 | FALLA | FALLA | PASA | PASA |
| 12 V | 1000 uF | 60.4 | 14.9 | 5 | 1/1 | 0.973 | 604 | 81.6 | 135 | 0.309 | FALLA | FALLA | FALLA | PASA |
| 25 V | 1000 uF | 146 | 20.5 | 5 | 5/5 | 1.96 | 3.04e+03 | 311 | 102 | 1.19 | FALLA | FALLA | FALLA | FALLA |

### 3.2 Remedios, en el orden pedido (BWK = 1, 9 combinaciones V_OUT x C_carga)
Cada fila cuenta cuantas de las 9 combinaciones cumplen cada criterio. "Las 3 a la vez" = caida <= 5 % + PGOOD no cae + sin reconmutacion.

**(1) C gate-drain de Q6 (RG = 10 k)** - `S3a_R1_cgd.log`

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| cgd=1e-08 | 2/9 | 3/9 | 9/9 | 2/9 | 7/9 | 9.97 | 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| cgd=2.2e-08 | 3/9 | 4/9 | 9/9 | 3/9 | 7/9 | 9.23 | 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| cgd=4.7e-08 | 6/9 | 6/9 | 9/9 | 6/9 | 7/9 | 9.05 | 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF |
| cgd=1e-07 | 8/9 | 8/9 | 9/9 | 8/9 | 7/9 | 9 | 25 V/1000 uF |

Primer valor que elimina la reconmutacion en las 9: **10 nF**. Ningun valor del rango 10-100 nF cumple las 3 a la vez; el mejor es 100 nF (8/9; falla 25 V/1000 uF: 8 A de carga + inrush > limite de 15 A, caida 9 %, PGOOD cae, sin reapertura de Q6). Extension rotulada, FUERA del rango pedido (`S3a_R1x_cgd_ext.log`):

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| cgd=2.2e-07 | 9/9 | 9/9 | 9/9 | 9/9 | 7/9 | 2.96 | - |
| cgd=4.7e-07 | 9/9 | 9/9 | 9/9 | 9/9 | 7/9 | 0.62 | - |
| cgd=1e-06 | 9/9 | 9/9 | 9/9 | 9/9 | 7/9 | 0.255 | - |

220 nF solo ya cumple las 3 (9/9), pero el apagado y la energia crecen mas (ver 3.5).

**(2) Subir R de gate (sin C gd)** - `S3a_R2_rg.log`

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| rg=2.2e+04 | 0/9 | 1/9 | 7/9 | 0/9 | 8/9 | 18.6 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rg=3.3e+04 | 0/9 | 2/9 | 7/9 | 0/9 | 8/9 | 17.2 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rg=4.7e+04 | 1/9 | 2/9 | 7/9 | 1/9 | 7/9 | 15.8 | 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rg=6.8e+04 | 2/9 | 3/9 | 7/9 | 2/9 | 7/9 | 14.3 | 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rg=1e+05 | 4/9 | 5/9 | 8/9 | 4/9 | 7/9 | 12.8 | 25 V/100 uF, 12 V/470 uF, 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF |

Ningun valor resuelve las 9. Ademas RG forma divisor con los 100 k G-S: V_GS(final) = -V_OUT x 100k/(100k+RG); a 7.2 V da -4.9 V con 47 k (< 5 V).

**(3) RC en la gate de Q7 (R serie desde PGOOD + C a GND)** - `S3a_R3_q7rc.log` y `S3a_R3b_q7rc_corto.log`

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| rq7=1e+05, cq7=4.7e-08 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=4.7e-08 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=1e+05, cq7=2.2e-07 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=2.2e-07 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=1e+05, cq7=1e-06 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=1e-06 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| rq7=1e+05, cq7=1e-09 | 0/9 | 0/9 | 7/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=1e-09 | 0/9 | 0/9 | 8/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=1e+05, cq7=4.7e-09 | 0/9 | 0/9 | 8/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=4.7e-09 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=1e+05, cq7=1e-08 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |
| rq7=2.2e+05, cq7=1e-08 | 0/9 | 0/9 | 9/9 | 0/9 | 7/9 | 65 | 7.2 V/100 uF, 12 V/100 uF, 25 V/100 uF, 7.2 V/470 uF, 12 V/470 uF, 25 V/470 uF, 7.2 V/1000 uF, 12 V/1000 uF, 25 V/1000 uF |

Solo enmascara el rearme (9/9 sin reconmutacion con tau >= ~1 ms) pero **no corrige la caida ni el PGOOD**, y la caida empeora (hasta 65 %) porque desaparece el chatteo que la limitaba. Descartado.

**Combinacion rotulada (1)+(2)** - `S3a_R12_cgd_rg.log`

| remedio | caida<=5 % | PGOOD no cae | sin reconmutacion | las 3 a la vez | SOA(t_eq)<=1 | caida max [%] | combinaciones que fallan alguna de las 3 |
|---|---|---|---|---|---|---|---|
| cgd=4.7e-08, rg=1.5e+04 | 6/9 | 7/9 | 9/9 | 6/9 | 7/9 | 9.01 | 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF |
| cgd=1e-07, rg=1.5e+04 | 8/9 | 8/9 | 9/9 | 8/9 | 7/9 | 9 | 25 V/1000 uF |
| cgd=4.7e-08, rg=2.2e+04 | 8/9 | 8/9 | 9/9 | 8/9 | 7/9 | 9 | 25 V/1000 uF |
| cgd=1e-07, rg=2.2e+04 | 9/9 | 9/9 | 9/9 | 9/9 | 7/9 | 2.19 | - |

**Remedio elegido: C gate-drain = 100 nF + RG = 22 k** (primer par de la lista que cumple las 3 en las 9 combinaciones; 100 nF/15 k y 47 nF/22 k fallan 25 V/1000 uF con 9 %). Ningun remedio aislado de los 3 resuelve todo dentro de los rangos pedidos; el elegido es una combinacion de (1) y (2) dentro de sus rangos.

### 3.3 Despues del remedio (C gd 100 nF + RG 22 k, BWK = 1) - `S3a_final.log`

| V_OUT | C_carga | I pico Q6 [A] | caida VOUT_INT [%] | PGOOD cae (n) | reapert./recierres Q6 | t encendido [ms] | P pico [W] | E Q6 [mJ] | t_eq [us] | util SOA(t_eq) | caida | PGOOD | reconm. | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 7.2 V | 100 uF | 7.97 | 0.325 | 0 | 0/0 | 5 | 15 | 54.3 | 3.63e+03 | 0.0618 | PASA | PASA | PASA | PASA |
| 12 V | 100 uF | 8.22 | 0.219 | 0 | 0/0 | 3.59 | 26.3 | 67.2 | 2.55e+03 | 0.112 | PASA | PASA | PASA | PASA |
| 25 V | 100 uF | 8.76 | 0.213 | 0 | 0/0 | 2.8 | 62.9 | 127 | 2.02e+03 | 0.576 | PASA | PASA | PASA | PASA |
| 7.2 V | 470 uF | 8.51 | 0.369 | 0 | 0/0 | 5.03 | 17.5 | 63.8 | 3.64e+03 | 0.0682 | PASA | PASA | PASA | PASA |
| 12 V | 470 uF | 9.42 | 0.616 | 0 | 0/0 | 3.61 | 36.3 | 93.6 | 2.58e+03 | 0.175 | PASA | PASA | PASA | PASA |
| 25 V | 470 uF | 11.8 | 0.774 | 0 | 0/0 | 2.82 | 135 | 242 | 1.79e+03 | 1.79 | PASA | PASA | PASA | FALLA |
| 7.2 V | 1000 uF | 9.29 | 0.701 | 0 | 0/0 | 5.05 | 21.5 | 77.4 | 3.6e+03 | 0.0882 | PASA | PASA | PASA | PASA |
| 12 V | 1000 uF | 11.1 | 1.07 | 0 | 0/0 | 3.63 | 57.8 | 131 | 2.27e+03 | 0.298 | PASA | PASA | PASA | PASA |
| 25 V | 1000 uF | 15.4 | 2.19 | 0 | 0/0 | 2.84 | 283 | 403 | 1.42e+03 | 3.14 | PASA | PASA | PASA | FALLA |

### 3.4 Sensibilidad a la velocidad de la fuente (caida / PGOOD / rearme)

| V_OUT / C_carga | base BWK=1 | base BWK=3 (3x lenta) | base BWK=1/3 (3x rapida) | final BWK=1 | final BWK=3 | final BWK=1/3 |
|---|---|---|---|---|---|---|
| 7.2 V / 100 uF | 9 % / PGOOD cae / sin rearme | 9.53 % / PGOOD cae / REARME | 5.61 % / PGOOD ok / sin rearme | 0.325 % / PGOOD ok / sin rearme | 1.26 % / PGOOD ok / sin rearme | 0.229 % / PGOOD ok / sin rearme |
| 7.2 V / 470 uF | 10.7 % / PGOOD cae / sin rearme | 13.1 % / PGOOD cae / REARME | 10.6 % / PGOOD cae / sin rearme | 0.369 % / PGOOD ok / sin rearme | 1.93 % / PGOOD ok / sin rearme | 0.243 % / PGOOD ok / sin rearme |
| 7.2 V / 1000 uF | 11.5 % / PGOOD cae / sin rearme | 14.3 % / PGOOD cae / REARME | 11.9 % / PGOOD cae / sin rearme | 0.701 % / PGOOD ok / sin rearme | 3 % / PGOOD ok / sin rearme | 0.264 % / PGOOD ok / sin rearme |
| 12 V / 100 uF | 9.12 % / PGOOD cae / sin rearme | 9.99 % / PGOOD cae / REARME | 9.11 % / PGOOD cae / sin rearme | 0.219 % / PGOOD ok / sin rearme | 1.11 % / PGOOD ok / sin rearme | 0.144 % / PGOOD ok / sin rearme |
| 12 V / 470 uF | 12.8 % / PGOOD cae / REARME | 15.1 % / PGOOD cae / REARME | 13.7 % / PGOOD cae / REARME | 0.616 % / PGOOD ok / sin rearme | 2.36 % / PGOOD ok / sin rearme | 0.162 % / PGOOD ok / sin rearme |
| 12 V / 1000 uF | 14.9 % / PGOOD cae / REARME | 16.9 % / PGOOD cae / REARME | 16.1 % / PGOOD cae / REARME | 1.07 % / PGOOD ok / sin rearme | 3.82 % / PGOOD ok / sin rearme | 0.242 % / PGOOD ok / sin rearme |
| 25 V / 100 uF | 9.27 % / PGOOD cae / sin rearme | 9.82 % / PGOOD cae / REARME | 9.28 % / PGOOD cae / sin rearme | 0.213 % / PGOOD ok / sin rearme | 0.891 % / PGOOD ok / sin rearme | 0.0738 % / PGOOD ok / sin rearme |
| 25 V / 470 uF | 16.9 % / PGOOD cae / REARME | 16.6 % / PGOOD cae / REARME | 17 % / PGOOD cae / REARME | 0.774 % / PGOOD ok / sin rearme | 2.58 % / PGOOD ok / sin rearme | 0.207 % / PGOOD ok / sin rearme |
| 25 V / 1000 uF | 20.5 % / PGOOD cae / REARME | 19.8 % / PGOOD cae / REARME | 20.7 % / PGOOD cae / REARME | 2.19 % / PGOOD ok / sin rearme | 4.39 % / PGOOD ok / sin rearme | 2.16 % / PGOOD ok / sin rearme |

Limite de C_carga a 25 V con el remedio (`S3a_final_lim25.log`; el SOA es el que manda). Las filas de 24.99 V son duplicados numericos de 25 V (artificio para tener dos pasos en `.step`):

| V_OUT | C_carga | I pico Q6 [A] | caida VOUT_INT [%] | PGOOD cae (n) | reapert./recierres Q6 | t encendido [ms] | P pico [W] | E Q6 [mJ] | t_eq [us] | util SOA(t_eq) | caida | PGOOD | reconm. | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 24.99 V | 150 uF | 9.17 | 0.301 | 0 | 0/0 | 2.81 | 69.9 | 142 | 2.04e+03 | 0.716 | PASA | PASA | PASA | AL LIMITE |
| 25 V | 150 uF | 9.17 | 0.301 | 0 | 0/0 | 2.81 | 70 | 142 | 2.04e+03 | 0.717 | PASA | PASA | PASA | AL LIMITE |
| 24.99 V | 220 uF | 9.73 | 0.419 | 0 | 0/0 | 2.81 | 80.4 | 164 | 2.04e+03 | 0.96 | PASA | PASA | PASA | AL LIMITE |
| 25 V | 220 uF | 9.74 | 0.419 | 0 | 0/0 | 2.81 | 80.5 | 164 | 2.04e+03 | 0.961 | PASA | PASA | PASA | AL LIMITE |
| 24.99 V | 330 uF | 10.6 | 0.586 | 0 | 0/0 | 2.81 | 98.5 | 198 | 2.01e+03 | 1.37 | PASA | PASA | PASA | FALLA |
| 25 V | 330 uF | 10.6 | 0.586 | 0 | 0/0 | 2.81 | 98.5 | 198 | 2.01e+03 | 1.38 | PASA | PASA | PASA | FALLA |

### 3.5 Efectos secundarios del remedio
- **Encendido**: de 0.13-1.96 ms a 2.8-5.05 ms (V_DS < 0.5 V).
- **Apagado** (S3b, 8 A, abriendo SW1): de 7.3-8.2 ms a 34.5 ms (12 V) / 57.5 ms (25 V) con 47 nF y 65 ms (12 V) / > 100 ms (25 V) con 100 nF. RG no esta en la ruta de apagado (SW1 abierto), por lo que valen tambien con RG = 22 k (no se re-simulo con RG = 22 k).
- **Disipacion**: energia de Q6 en el encendido 54-403 mJ (antes 15-311 mJ); en el apagado con 8 A 0.39-0.42 J (12 V, 47 nF) / 0.81-0.84 J (12 V, 100 nF) y 1.64 J (47 nF) a > 2.9 J (100 nF, sin terminar en 100 ms) a 25 V, contra 0.027-0.088 J sin remedio.
- **SOA**: a 25 V el encendido con C >= 330 uF y el apagado con 8 A (todas las cargas) **superan la SOA del datasheet** (u = 1.4-3.1). A <= 12 V todo pasa (u <= 0.59). Sin remedio, el apagado a 25 V solo llega a u = 0.72.
- **V_GS a 6.8 V**: de -6.17 V a -5.56 V (25 C) / -5.47 V (100 C): margen de 0.47 V sobre el minimo de 5 V.

## 4. S3b - apertura con 8 A y carga inductiva

Sin diodo, sin remedio (`S3b_apertura.log`):

| V_OUT | L carga | V_DS max Q6 [V] | VOUT_EXT min [V] | sobreimpulso VOUT_INT [%] | t V_GS=-2.5 V [ms] | t I<10% [ms] | E Q6 [mJ] | P pico [W] | t_eq [ms] | util SOA(t_eq) | V_DS<32 V | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 V | 0.5 uH | 12 | 3.55e-10 | 0.447 | 7.04 | 7.27 | 26.6 | 24.2 | 1.1 | 0.0665 | PASA | PASA |
| 25 V | 0.5 uH | 25 | -2.78e-06 | 0.136 | 7.55 | 7.95 | 55.3 | 50.1 | 1.1 | 0.219 | PASA | PASA |
| 12 V | 1 uH | 12 | 3.55e-10 | 0.447 | 7.04 | 7.27 | 26.6 | 24.2 | 1.1 | 0.0665 | PASA | PASA |
| 25 V | 1 uH | 25 | -5.68e-06 | 0.136 | 7.55 | 7.95 | 55.3 | 50.1 | 1.1 | 0.219 | PASA | PASA |
| 12 V | 100 uH | 12.2 | -0.113 | 0.464 | 7.09 | 7.31 | 29.3 | 28.6 | 1.03 | 0.0647 | PASA | PASA |
| 25 V | 100 uH | 25 | -6.37e-05 | 0.138 | 7.58 | 7.97 | 58 | 53.2 | 1.09 | 0.247 | PASA | PASA |
| 12 V | 1 mH | 21.8 | -9.78 | 0.416 | 7.36 | 7.66 | 57.5 | 58.9 | 0.976 | 0.229 | PASA | PASA |
| 25 V | 1 mH | 30.7 | -5.7 | 0.14 | 7.76 | 8.2 | 87.5 | 79.4 | 1.1 | 0.716 | AL LIMITE | AL LIMITE |

Con Schottky MBR745 en VOUT_EXT, sin remedio (`S3b_apertura_dfw.log`):

| V_OUT | L carga | V_DS max Q6 [V] | VOUT_EXT min [V] | sobreimpulso VOUT_INT [%] | t V_GS=-2.5 V [ms] | t I<10% [ms] | E Q6 [mJ] | P pico [W] | t_eq [ms] | util SOA(t_eq) | I pico diodo [A] | V_DS<32 V | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 V | 0.5 uH | 12 | -3.74e-10 | 0.447 | 7.04 | 7.27 | 26.6 | 24.2 | 1.1 | 0.0666 | 1.8e-09 | PASA | PASA |
| 25 V | 0.5 uH | 25 | -5.93e-07 | 0.136 | 7.55 | 7.95 | 55.3 | 50.1 | 1.1 | 0.219 | 2.17e-08 | PASA | PASA |
| 12 V | 1 uH | 12 | -8.72e-07 | 0.447 | 7.04 | 7.27 | 26.6 | 24.2 | 1.1 | 0.0665 | 8.23e-09 | PASA | PASA |
| 25 V | 1 uH | 25 | -1.79e-06 | 0.136 | 7.55 | 7.95 | 55.3 | 50.1 | 1.1 | 0.219 | 3.49e-08 | PASA | PASA |
| 12 V | 100 uH | 12.2 | -0.109 | 0.464 | 7.09 | 7.31 | 29.3 | 28.6 | 1.03 | 0.0647 | 0.0016 | PASA | PASA |
| 25 V | 100 uH | 25 | -8.91e-06 | 0.138 | 7.58 | 7.97 | 58 | 53.2 | 1.09 | 0.247 | 2.34e-08 | PASA | PASA |
| 12 V | 1 mH | 12.7 | -0.526 | 1.54 | 7.27 | 7.34 | 40.6 | 58.3 | 0.696 | 0.131 | 3.52 | PASA | PASA |
| 25 V | 1 mH | 25.5 | -0.442 | 0.384 | 7.76 | 8.04 | 80 | 79.4 | 1.01 | 0.659 | 1.41 | PASA | PASA |

**Diodo de rueda libre**: con el apagado lento (100 k contra C_iss, 7-8 ms) Q6 se autolimita y V_DS < 32 V en todos los casos, pero a 25 V/1 mH llega a 30.73 V (AL LIMITE: > 25.6 V; margen 1.3 V) y VOUT_EXT cae a -9.78 V (12 V/1 mH). Recomendacion: el diodo queda **AL LIMITE, recomendado (u 0.716 vs 0.7)** para cargas >= ~1 mH a V_OUT alto sin el remedio (montar o DNP: decision de Luis, ver §8.6 y §10.9); con el remedio de gate (apagado de decenas de ms) ya no hace falta (V_DS <= 25 V, VOUT_EXT >= -0.14 V). Si se monta: Schottky >= 40 V; la corriente medida por el diodo es 3.5 A pico en el peor caso, la clase de corriente se elige por la carga real.

Con remedio C gd (47 nF y 100 nF; RG no interviene), sin diodo (`S3b_apertura_R1.log`):

| V_OUT | L carga | C gd | V_DS max Q6 [V] | VOUT_EXT min [V] | sobreimpulso VOUT_INT [%] | t V_GS=-2.5 V [ms] | t I<10% [ms] | E Q6 [mJ] | P pico [W] | t_eq [ms] | util SOA(t_eq) | V_DS<32 V | SOA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 V | 0.5 uH | 47 nF | 12 | 1.7e-09 | 0.00646 | 26.7 | 34.5 | 393 | 24 | 16.4 | 0.322 | PASA | PASA |
| 25 V | 0.5 uH | 47 nF | 25 | 1.87e-07 | 0.0013 | 39.2 | 57.5 | 1.64e+03 | 50 | 32.7 | 1.53 | PASA | FALLA |
| 12 V | 1 uH | 47 nF | 12 | 1.7e-09 | 0.00646 | 26.7 | 34.5 | 393 | 24 | 16.4 | 0.322 | PASA | PASA |
| 25 V | 1 uH | 47 nF | 25 | 1.87e-07 | 0.0013 | 39.2 | 57.5 | 1.64e+03 | 50 | 32.7 | 1.53 | PASA | FALLA |
| 12 V | 100 uH | 47 nF | 12 | -6.42e-05 | 0.00664 | 26.8 | 34.5 | 396 | 24.1 | 16.4 | 0.325 | PASA | PASA |
| 25 V | 100 uH | 47 nF | 25 | -4.54e-05 | 0.00133 | 39.2 | 57.6 | 1.64e+03 | 50 | 32.8 | 1.54 | PASA | FALLA |
| 12 V | 1 mH | 47 nF | 12.1 | -0.143 | 0.00796 | 27.3 | 35.1 | 424 | 25.3 | 16.7 | 0.354 | PASA | PASA |
| 25 V | 1 mH | 47 nF | 25 | -0.0238 | 0.00149 | 39.4 | 57.8 | 1.67e+03 | 50.6 | 32.9 | 1.56 | PASA | FALLA |
| 12 V | 0.5 uH | 100 nF | 12 | 2.98e-06 | 0.00215 | 48.9 | 65.2 | 806 | 24 | 33.6 | 0.564 | PASA | PASA |
| 25 V | 0.5 uH | 100 nF | 18.8 | 6.18 | -0.0145 | 74.9 | n/d | 2.88e+03 | 50 | 57.6 | 1.88 | PASA | FALLA |
| 12 V | 1 uH | 100 nF | 12 | 2.98e-06 | 0.00215 | 48.9 | 65.2 | 806 | 24 | 33.6 | 0.564 | PASA | PASA |
| 25 V | 1 uH | 100 nF | 18.8 | 6.18 | -0.0145 | 74.9 | n/d | 2.88e+03 | 50 | 57.6 | 1.88 | PASA | FALLA |
| 12 V | 100 uH | 100 nF | 12 | -3.33e-05 | 0.00223 | 48.9 | 65.2 | 809 | 24 | 33.7 | 0.567 | PASA | PASA |
| 25 V | 100 uH | 100 nF | 18.8 | 6.18 | -0.0145 | 74.9 | n/d | 2.88e+03 | 50 | 57.6 | 1.88 | PASA | FALLA |
| 12 V | 1 mH | 100 nF | 12 | -0.0341 | 0.00277 | 49.4 | 65.8 | 837 | 24.6 | 34 | 0.586 | PASA | PASA |
| 25 V | 1 mH | 100 nF | 18.8 | 6.18 | -0.0147 | 75.1 | n/d | 2.9e+03 | 50.3 | 57.6 | 1.89 | PASA | FALLA |

Con diodo y remedio (`S3b_apertura_dfw_R1.log`): resultados identicos salvo corriente de diodo ~ 0 A (el remedio ya evita el pico negativo).

## 5. S3c - niveles en regimen (8 A)

Sin remedio (`S3c_niveles.log`):

| V_OUT | T [C] | V_GS Q6 [V] | V_GS Q7 [V] | V_DS Q6 [mV] | I Q6 [A] | P Q6 [W] | Rds eq [mohm] | P zener [mW] | P R 4.7k LED2 [mW] | |V_GS Q6| 5-15 V | P<=0.5 W |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 6.8 V | 25 | -6.17 | 7.35 | 15.3 | 8 | 0.122 | 1.91 | 2.76e-06 | 5.45 | PASA | PASA |
| 12 V | 25 | -10.9 | 7.35 | 13.5 | 8 | 0.108 | 1.69 | 0.00194 | 22.3 | PASA | PASA |
| 25 V | 25 | -12 | 7.35 | 13.3 | 8 | 0.107 | 1.67 | 14.1 | 114 | PASA | PASA |
| 6.8 V | 100 | -6.12 | 7.35 | 20.7 | 8 | 0.166 | 2.59 | 0.0312 | 6.73 | PASA | PASA |
| 12 V | 100 | -10.8 | 7.35 | 18.5 | 8 | 0.148 | 2.31 | 0.0632 | 24.8 | PASA | PASA |
| 25 V | 100 | -12 | 7.35 | 18.3 | 8.01 | 0.146 | 2.28 | 14.1 | 120 | PASA | PASA |

Con C gd 100 nF + RG 22 k (`S3c_niveles_final.log`):

| V_OUT | T [C] | V_GS Q6 [V] | V_GS Q7 [V] | V_DS Q6 [mV] | I Q6 [A] | P Q6 [W] | Rds eq [mohm] | P zener [mW] | P R 4.7k LED2 [mW] | |V_GS Q6| 5-15 V | P<=0.5 W |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 6.8 V | 25 | -5.56 | 7.35 | 15.9 | 8 | 0.127 | 1.99 | 2.48e-06 | 5.45 | PASA | PASA |
| 12 V | 25 | -9.82 | 7.35 | 13.7 | 8 | 0.11 | 1.71 | 4.85e-06 | 22.3 | PASA | PASA |
| 25 V | 25 | -11.9 | 7.35 | 13.4 | 8 | 0.107 | 1.67 | 5.66 | 114 | PASA | PASA |
| 6.8 V | 100 | -5.47 | 7.35 | 21.6 | 8 | 0.172 | 2.69 | 0.0279 | 6.73 | PASA | PASA |
| 12 V | 100 | -9.73 | 7.35 | 18.8 | 8 | 0.15 | 2.34 | 0.0497 | 24.8 | PASA | PASA |
| 25 V | 100 | -11.9 | 7.35 | 18.3 | 8.01 | 0.146 | 2.28 | 5.66 | 120 | PASA | PASA |

Nota: a 25 V la resistencia de 4.7 k de LED2/LED3 disipa 114-120 mW (limite de un 0603 = 100 mW): usar 0805 (>= 0.125 W) o subir R.

## 6. S3d - bleeder (Q6 abierto, se quita la fuente a 10 ms)

| V_OUT | t PGOOD cae [ms] | t a 5 V [s] | t a 2 V [s] | t a 1 V [s] | P bleeder reg [W] | P total reg [W] (bleeder+FB+LED2) |
|---|---|---|---|---|---|---|
| 12 V | 50.1 | 0.575 | 1.19 | 1.65 | 0.096 | 0.125 |
| 25 V | 49.1 | 1.07 | 1.68 | 2.14 | 0.417 | 0.552 |

## 7. S3e - descarga de C_IN (100 uF/31 mohm + 10 uF + 100 nF) por LED1 + 4.7 k y divisor EN 200 k + 66.5 k

| V_BAT | t a 10 V [s] | t a 5 V [s] | t a 3 V [s] | t a 2 V [s] | t a 1 V [s] | t a 0.5 V [s] | LED1 < 1 mA a [s] | LED1 < 10 uA a [s] (V) |
|---|---|---|---|---|---|---|---|---|
| 12 V | 0.11 | 0.577 | 1.04 | 1.67 | 14.8 | 35.3 | 0.398 | 2.84 (1.57 V) |
| 25.2 V | 0.53 | 0.997 | 1.46 | 2.09 | 15.3 | 35.8 | 0.818 | 3.26 (1.57 V) |

Sin LM5175 ni carga (peor caso lento): con el convertidor consumiendo, la descarga es mucho mas rapida.

## 8. Cambios para `PCB_Potencia_Mejoras_Rev2.md`
> **Nota sobre §8.1**: la recomendacion de C_gd directo (100 nF + RG 22 k) es la del primer informe. La recomendacion vigente es la de **§10.9 (decision 1)**: diodo en serie con C_gd + `Rrst` 330 k + R_G 22 k, con retardo de recierre >= 200 ms, a reserva de la decision de Luis.

1. Q6: agregar **C gate-drain 100 nF** (X7R >= 50 V) y subir el serie de gate de 10 k a **22 k** (V_GS a 6.8 V = -5.5 V; no subir mas de ~27 k). **Ver §10.9 (decision 1)**: esta es la recomendacion del primer informe (C_gd directo); la variante con diodo en serie + `Rrst` de §10 apaga en ~8 ms pero deja el recierre antes de ~200 ms sin encendido lento, y la de C_gd directo apaga en 65 ms / > 100 ms con SOA de apagado en FALLA a 25 V/8 A. La eleccion entre ambas queda para Luis.
2. Limitar C_carga a 25 V: **limite recomendado 100-150 uF** (100 uF PASA, u = 0.576; **150 uF AL LIMITE, u = 0.717**), **~200 uF como cota** (u = 1; 220 uF u = 0.96, 270 uF FALLA), misma cifra que §10.8/§10.9 (decision 3); la decision final es de Luis. Mas exige precarga/arranque suave o cambiar de FET; a <= 12 V se admiten 1000 uF.
3. Con el remedio de C_gd directo (100 nF) el apagado con 8 A dura 65 ms a 12 V y > 100 ms a 25 V, y a 25 V supera la SOA (u = 1.88, §4). Un seguidor de apagado en la gate seria un **NPN** (colector a source, emisor a gate), no un PNP; no se simulo. "No abrir Q6 con 8 A a 25 V" **no es exigible**: PGOOD tambien abre Q6, asi que esa apertura ocurre en operacion normal (S3b). La alternativa simulada en §10 (C_gd con diodo en serie) recupera un apagado de ~8 ms, con las salvedades de 10.2 (V_GS) y 10.5 (recierre).
4. LED1/LED2/LED3: 4.7 k a 25 V disipa ~0.12 W; usar 0805 o >= 6.8 k.
5. Bleeder 1.5 k: 0.417 W a 25 V (2512 de 1 W: ok).
6. Diodo de rueda libre en VOUT_EXT: **AL LIMITE, recomendado (u 0.716 vs 0.7)** para cargas >= ~1 mH a 25 V con apagado rapido (§10.3); con el remedio de C_gd directo (§8.1) no hace falta. Montarlo o dejarlo DNP es **decision de Luis** (§10.9, decision 2).

## 9. Supuestos y limitaciones
- Convertidor idealizado (L_s || R_d + limitador 15 A): no reproduce soft-start, limite de corriente por ciclo ni hiccup del LM5175; la caida depende de BWK (se reporta con 3 velocidades). La fuente 3x mas lenta empeora la caida (max 4.39 % con remedio).
- PGOOD sin deglitch (peor caso); el LM5175 real puede filtrar glitches. Umbrales del enunciado, no del datasheet del LM5175.
- ESR de C_carga (10 mohm) supuesta; inductancia de cableado y ESL no incluidas; C_carga descargada en t = 0 y R de carga de 8 A al nominal.
- Zener, LED y Schottky: modelos genericos de `standard.dio`, no las piezas reales (MM3Z12VT1G, LED real, B5819W / Schottky de rueda libre).
- Q6 (Vishay nivel 3): sin autocalentamiento; la "T" de S3c es temperatura de union impuesta. SOA del datasheet a TC = 25 C (optimista si la placa calienta), digitalizada (+-10 %), pulso equivalente E/P_pico; no se uso Zth transitorio.
- Contadores de reconmutacion saturados en 5 (leer ">= 5"). El apagado de 100 nF a 25 V no termina en 100 ms (energia y t apagado son cotas inferiores).
- No se incluye la carga del LM5175 (BIAS/VOSNS) en S3d/S3e.
- Ejecucion: otros bancos corren LTspice en paralelo; se uso una copia privada del ejecutable y reintento si el log no terminaba (`run.sh`). Todos los `.log` citados terminan con "Total elapsed time" y tienen sus bloques `.meas`.

## 10. Seguimiento (revision) - S3f

Todos los numeros salen de `.meas` de los `S3f_*.log`; las tablas las genera `S3f_tablas.py` (usa `S3f_post.py` y `s3_post.py`, que solo leen los `.log`). El circuito es `S3f_common.inc` (copia de `common_S3.inc` con el remedio nuevo; `common_S3.inc` y los bancos S3a-S3e no se tocaron). Validez de los logs: LTspice 26 escribe "Total elapsed time" **antes** de las tablas `.meas` (no es la ultima linea del archivo); el criterio usado fue una sola linea "Total elapsed time" y todas las tablas `.meas` con el numero de pasos esperado. Los 31 logs `S3f_*.log` lo cumplen. Corridas que fallaron o salieron incompletas y se repitieron: `S3f_soa25` y las dos de precarga (LTspice rechazo `.step param VN list 25` de un solo valor y el `.param VN` duplicado de `S3f_common.inc`; se quito el valor por defecto de VN del `.inc`, ninguna corrida anterior lo usaba), y `S3f_rejilla_R10k_RG2k2` (dos intentos con log incompleto, causa no identificada; el tercero termino). Los bloques `.meas` `fbmin` y `gon1` traen filas extra al final, igual que `S3a_final.log`; no se usan en ningun veredicto. Ejecucion: cadenas secuenciales de a 2 `LTspice.exe -b` (por unos 2 minutos hubo 3 a la vez).

### 10.1 Montaje del remedio con encendido lento y apagado rapido

- Red de gate de Q6: drenaje - **C_gd 100 nF** - nodo `m` - **diodo 1N4148** (anodo en `m`, catodo en la gate) y **R de reposicion `Rrst`** de `m` a source (VOUT_INT); R_G = 22 k, 100 k G-S y zener como en §3.3. Modelo del diodo: `.model D1N4148 D(Is=2.52n Rs=.568 N=1.752 Cjo=4p M=.4 tt=20n ...)`, copia literal de `standard.dio` (no es la pieza real).
- Encendido: dV_D/dt > 0 inyecta corriente a la gate por C_gd + diodo (Miller lento, igual que en §3.3). Apagado: el diodo bloquea y la gate sube solo por los 100 k G-S.
- Efecto estatico que el enunciado no preveia: con Q6 cerrado el nodo `m` se queda a 0.6 V sobre la gate, asi que `Rrst` + diodo queda **en paralelo con los 100 k G-S** y forma un divisor con R_G: V_GS baja (10.2).
- Control de que el montaje es el pedido: con `Rrst` = 1 G el V_GS a 6.8 V es -5.558 V (25 C) y -5.467 V (100 C), igual que `S3c_niveles_final.log`; y `S3f_ilim_viejo_Rd.log` con ILIM = 15 A (red vieja, C_gd directo) reproduce `S3a_final.log` (caida 2.19 % a 25 V/1000 uF, 0.616 % a 12 V/470 uF).
- Recierre: GAP = tiempo desde la **orden** de apertura de SW1 hasta la orden de cierre (Q6 tarda 2-8 ms en abrir; a 1 y 5 ms Q6 todavia no abrio). Las filas "V(m)-V(vint)" miden cuanto del nodo `m` queda sin reponer al recierre.

### 10.2 S3c - V_GS en regimen (8 A, 6.8/12/25 V, 25 y 100 C) segun `Rrst`

| Rrst a source | V_GS Q6 a 6.8 V, 25 C [V] | idem 6.8 V, 100 C [V] | idem 12 V, 25 C [V] | idem 25 V, 25 C [V] | P(Q6) max [W] | P(Q6) a 6.8 V, 100 C [W] | abs(V_GS) >= 5 V en todo | P <= 0.5 W |
|---|---|---|---|---|---|---|---|---|
| 10 k | -2.55 | -2.31 | -3.85 | -7.67 | 169 (artefacto) | 101 (artefacto) | FALLA | FALLA |
| 47 k | -4.15 | -4.03 | -7.24 | -11.8 | 0.203 | 0.203 | FALLA | PASA |
| 100 k | -4.78 | -4.67 | -8.39 | -11.9 | 0.185 | 0.185 | FALLA | PASA |
| 220 k | -5.17 | -5.07 | n/d | -11.9 | 0.178 | 0.178 | PASA | PASA |
| 330 k | -5.29 | -5.19 | n/d | -11.9 | 0.176 | 0.176 | PASA | PASA |
| 470 k | -5.37 | -5.27 | n/d | -11.9 | 0.175 | 0.175 | PASA | PASA |
| 1000 k | -5.47 | -5.37 | n/d | -11.9 | 0.174 | 0.174 | PASA | PASA |
| ninguna | -5.56 | -5.47 | -9.82 | -11.9 | 0.172 | 0.172 | PASA | PASA |

(`S3f_niveles.log` para 10 k/47 k/100 k/ninguna y `S3f_niveles_ext.log` para 220 k-1 M; "n/d" = punto no corrido.) El minimo de |V_GS| es 5 V (§5): **10 k, 47 k y 100 k FALLAN** a 6.8 V (-2.31 / -4.03 / -4.67 V a 100 C); con 10 k Q6 ni se enciende bien (V_GS -2.3 a -2.6 V a 6.8 V, zona lineal). Los 101-169 W de P(Q6) de esa fila son un **artefacto** de la carga ideal de 8 A (`Iload` es una fuente de corriente que fuerza 8 A aunque Q6 apenas conduzca, asi que V_DS x 8 A sale en cientos de W): **no son potencia fisica y no deben citarse como tal**; el hecho que si vale es que V_GS no llega a 5 V. A partir de 220 k pasa, con margen de 0.07 V (220 k), 0.19 V (330 k) y 0.27 V (470 k) a 6.8 V/100 C. P(Q6) <= 0.2 W en los que pasan.

### 10.3 S3b - apertura a 25 V/8 A con L 0.5 uH y 1 mH (`S3f_apertura.log`, `S3f_apertura_dfw.log`)

| Rrst | V_OUT | L | t apagado I<10% [ms] | V_DS max sin D [V] | VOUT_EXT min sin D [V] | util SOA sin D | V_DS max con Schottky [V] | util SOA con D | I pico D [A] |
|---|---|---|---|---|---|---|---|---|---|
| 10 k | 25 V | 0.5 uH | 3.69 | 25 | 6.94e-08 | 0.205 | 25 | 0.205 | -2.71e-10 |
| 10 k | 25 V | 1 mH | 3.94 | 30.7 | -5.69 | 0.697 | 25.5 | 0.647 | 1.41 |
| 47 k | 25 V | 0.5 uH | 7.94 | 25 | 7.42e-08 | 0.22 | 25 | 0.22 | -2.73e-10 |
| 47 k | 25 V | 1 mH | 8.18 | 30.7 | -5.68 | 0.716 | 25.5 | 0.66 | 1.41 |
| 100 k | 25 V | 0.5 uH | 7.95 | 25 | 1.87e-07 | 0.22 | 25 | 0.22 | -6.51e-10 |
| 100 k | 25 V | 1 mH | 8.19 | 30.7 | -5.68 | 0.716 | 25.5 | 0.66 | 1.41 |
| 220 k | 25 V | 0.5 uH | 7.95 | 25 | 7.75e-06 | 0.22 | 25 | 0.22 | -2.68e-08 |
| 220 k | 25 V | 1 mH | 8.19 | 30.7 | -5.68 | 0.716 | 25.5 | 0.66 | 1.41 |
| 470 k | 25 V | 0.5 uH | 7.95 | 25 | 3.37e-05 | 0.22 | 25 | 0.22 | -1.17e-07 |
| 470 k | 25 V | 1 mH | 8.19 | 30.7 | -5.68 | 0.716 | 25.5 | 0.66 | 1.41 |

(Tambien a 12 V en los `.log`: V_DS max 12 V / 21.8-21.9 V (1 mH), VOUT_EXT min -9.8 V a 1 mH sin diodo, util SOA <= 0.23.) El apagado vuelve a ser el de §4 sin remedio: **t(I<10 %) = 7.94-8.19 ms con `Rrst` >= 47 k (3.7-3.9 ms con 10 k)**, contra 65 ms (12 V) y > 100 ms (25 V) con C_gd directo. SOA de apagado a 25 V: u = 0.205-0.22 con L = 0.5 uH (PASA), 0.697 (10 k) / 0.716 (>= 47 k) con 1 mH sin diodo (AL LIMITE), 0.647-0.66 con diodo.

**Diodo de rueda libre**: **AL LIMITE, recomendado (u 0.716 vs 0.7) para cargas >= ~1 mH a 25 V; no hace falta para 0.5 uH (ni cable de 0.5/1 uH)**. Montarlo o DNP es decision de Luis (§10.9, decision 2). Con L = 1 mH y 25 V, sin diodo la utilizacion SOA es u = 0.716 (`Rrst` >= 47 k) > 0.7 y V_DS max = 30.7 V (< 32 V pero > 25.6 V: AL LIMITE por la regla de margen del 20 %; margen 1.3 V) y VOUT_EXT cae a -5.7 V (-9.8 V a 12 V/1 mH); con Schottky MBR745 u = 0.66 (< 0.7), V_DS = 25.5 V y VOUT_EXT >= -0.53 V (I pico del diodo 1.41 A a 25 V, 3.5 A a 12 V). Con 0.5 uH, u = 0.22 y V_DS = 25 V sin diodo. Para L = 100 uH solo existe el dato de §4 (sin remedio, apagado equivalente: V_DS = 25 V a 25 V, 12.2 V a 12 V); no se corrio con la red nueva. La frase de §4/§8.6 ("con el remedio ya no hace falta") solo vale para la red de C_gd directo, que apaga lento.

### 10.4 Rejilla de S3a (VN 7.2/12/25 V x C_carga 100/470/1000 uF x BWK 1/3/1/3; 27 casos por `Rrst`)

| Rrst | combinaciones que pasan caida+PGOOD+reconm. (de 27) | caida max BWK 1 / 3 / 1/3 [%] | util SOA > 1 (BWK=1) | V_GS final a 7.2 V [V] | observacion |
|---|---|---|---|---|---|
| 10 k | 27/27 | 1.07 / 3.53 / 0.273 | 25 V/470 uF (u=1.76), 25 V/1000 uF (u=3.12) | -2.43 | FALSO PASA a 7.2 V: Q6 no cierra (I pico 0.79 A, nunca V_DS < 0.5 V); V_GS a 12 V = -3.85 V |
| 47 k | 27/27 | 1.45 / 4.2 / 1.42 | 25 V/470 uF (u=1.79), 25 V/1000 uF (u=3.14) | -4.39 | V_GS a 7.2 V < 5 V |
| 100 k | 27/27 | 1.82 / 4.3 / 1.79 | 25 V/470 uF (u=1.79), 25 V/1000 uF (u=3.14) | -5.06 |  |
| 330 k | 27/27 | 2.06 / 4.36 / 2.03 | 25 V/470 uF (u=1.79), 25 V/1000 uF (u=3.14) | -5.6 |  |

Los criterios del banco (caida <= 5 %, PGOOD no cae, sin reconmutacion) pasan 27/27 con las cuatro `Rrst`, como con el remedio de §3.3, pero con 10 k **solo 18/27 son validos** (los 9 casos de 7.2 V son falsos positivos, ver abajo); advertencias: (1) con **10 k Q6 no cierra a 7.2 V** (V_GS final -2.43 V, I pico 0.79 A, no hay t encendido): la fila "pasa" porque la carga nunca se conecta (falso positivo); (2) con 47 k a 7.2 V el V_GS final (-4.39 V) queda bajo 5 V, como en 10.2. El encendido es igual que en §3.3-3.5: la SOA a 25 V sigue FALLANDO con 470 uF (u = 1.76-1.79) y 1000 uF (u = 3.12-3.14); a <= 12 V todo u <= 0.31.

### 10.5 Recierre (S3a con Q6 cerrado y 8 A, apertura en t = 40 ms y recierre GAP despues de la orden; `S3f_recierre_R*.log`)

| Rrst | GAP [ms] | pasan las 3 (de 6) | fallan | I pico max [A] | caida max [%] | util SOA > 1 |
|---|---|---|---|---|---|---|
| ninguna | 1 | 6/6 | - | 8 | 0.133 | - |
| ninguna | 5 | 6/6 | - | 11.6 | 1.51 | - |
| ninguna | 20 | 0/6 | 12 V/100 uF, 25 V/100 uF, 12 V/470 uF, 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF | 108 | 18.6 | 25 V/1000 uF (2.55) |
| 10 k | 1 | 6/6 | - | 10.1 | 1.3 | - |
| 10 k | 5 | 3/6 | 25 V/100 uF, 25 V/470 uF, 25 V/1000 uF | 80.6 | 6.3 | 25 V/100 uF (1.53), 25 V/470 uF (1.06) |
| 10 k | 20 | 6/6 | - | 14.6 | 1.07 | 25 V/470 uF (1.75), 25 V/1000 uF (3.09) |
| 47 k | 1 | 6/6 | - | 8 | 0.135 | - |
| 47 k | 5 | 5/6 | 12 V/1000 uF | 18.9 | 8.24 | - |
| 47 k | 20 | 5/6 | 25 V/470 uF | 45 | 3.56 | 25 V/470 uF (2.88), 25 V/1000 uF (5.41) |
| 100 k | 1 | 6/6 | - | 8 | 0.134 | - |
| 100 k | 5 | 6/6 | - | 17.2 | 4.52 | - |
| 100 k | 20 | 0/6 | 12 V/100 uF, 25 V/100 uF, 12 V/470 uF, 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF | 108 | 18.5 | 25 V/1000 uF (3.26) |
| 330 k | 1 | 6/6 | - | 8 | 0.133 | - |
| 330 k | 5 | 6/6 | - | 13.9 | 2.65 | - |
| 330 k | 20 | 0/6 | 12 V/100 uF, 25 V/100 uF, 12 V/470 uF, 25 V/470 uF, 12 V/1000 uF, 25 V/1000 uF | 108 | 18.6 | 25 V/1000 uF (2.57) |

La fila "ninguna" (`Rrst` = 1 G, el nodo `m` nunca se repone) es el control: a 20 ms **0/6** pasan (caida 9-18.6 %, I pico hasta 108 A), es decir se pierde por completo el encendido lento. Lo que importa es cuanto del nodo `m` se repone antes del recierre: el offset V(m)-V(vint) mueve el V_GS al que empieza a actuar el Miller; offsets de 2.4-3.5 V (47 k, 25 V) ya dan I pico de 13.5-45 A. La constante de tiempo `Rrst` x C_gd = 1 / 4.7 / 10 / 33 ms (10 k/47 k/100 k/330 k):

- A 1 ms Q6 aun no abrio: 6/6 pasan con todas las `Rrst`. A 5 ms Q6 esta a medio abrir: 10 k falla 3/6 (25 V: caida hasta 6.3 %, I pico 80.6 A) y 47 k falla 12 V/1000 uF (caida 8.24 %).
- A 20 ms solo **10 k pasa los tres criterios (6/6)** y aun asi u > 1 a 25 V/470 y 1000 uF (SOA, igual que en 10.4); 47 k pasa 5/6 pero con I pico 45 A y u = 2.88-5.41 a 25 V; **100 k y 330 k fallan 0/6, igual que sin reposicion**.
- Con 330 k (la unica de las barridas compatible con 10.2) el recierre solo es seguro con GAP grande (`S3f_recierre_R330k_largo.log`):

| V_OUT | C_carga | GAP [ms] | V(m)-V(vint) al recierre [V] | I pico [A] | caida [%] | PGOOD cae (n) | reaperturas (n) | util SOA | caida / PGOOD / reconm. |
|---|---|---|---|---|---|---|---|---|---|
| 12 V | 470 uF | 50 | -5.18 | 41.4 | 10.3 | 1 | 1 | 0.289 | FALLA / FALLA / FALLA |
| 25 V | 470 uF | 50 | -9.59 | 104 | 13.4 | 2 | 1 | 0.738 | FALLA / FALLA / FALLA |
| 12 V | 1000 uF | 50 | -5.24 | 43.4 | 12.9 | 2 | 0 | 0.409 | FALLA / FALLA / PASA |
| 25 V | 1000 uF | 50 | -9.93 | 108 | 18.5 | 3 | 2 | 3.43 | FALLA / FALLA / FALLA |
| 12 V | 470 uF | 100 | -1.14 | 9.39 | 0.609 | 0 | 0 | 0.17 | PASA / PASA / PASA |
| 25 V | 470 uF | 100 | -2.11 | 11.7 | 0.837 | 0 | 1 | 2.15 | PASA / PASA / FALLA |
| 12 V | 1000 uF | 100 | -1.15 | 11.1 | 1.05 | 0 | 0 | 0.292 | PASA / PASA / PASA |
| 25 V | 1000 uF | 100 | -2.18 | 15.4 | 2.06 | 0 | 0 | 3.14 | PASA / PASA / PASA |
| 12 V | 470 uF | 200 | -0.0542 | 9.39 | 0.609 | 0 | 0 | 0.17 | PASA / PASA / PASA |
| 25 V | 470 uF | 200 | -0.101 | 11.7 | 0.771 | 0 | 0 | 1.78 | PASA / PASA / PASA |
| 12 V | 1000 uF | 200 | -0.0548 | 11.1 | 1.05 | 0 | 0 | 0.292 | PASA / PASA / PASA |
| 25 V | 1000 uF | 200 | -0.105 | 15.4 | 2.06 | 0 | 0 | 3.11 | PASA / PASA / PASA |
| 12 V | 470 uF | 400 | -0.000135 | 9.39 | 0.609 | 0 | 0 | 0.17 | PASA / PASA / PASA |
| 25 V | 470 uF | 400 | -0.000259 | 11.7 | 0.771 | 0 | 0 | 1.78 | PASA / PASA / PASA |
| 12 V | 1000 uF | 400 | -0.000137 | 11.1 | 1.05 | 0 | 0 | 0.292 | PASA / PASA / PASA |
| 25 V | 1000 uF | 400 | -0.000267 | 15.4 | 2.06 | 0 | 0 | 3.11 | PASA / PASA / PASA |

  A 50 ms falla 4/4; a 100 ms falla 25 V/470 uF (1 reapertura, u = 2.15) y el resto pasa; **a >= 200 ms pasa todo**, con V(m)-V(vint) <= 0.11 V. La u de SOA a 25 V sigue siendo 1.78 (470 uF) y 3.11 (1000 uF), como en 10.4.

### 10.6 Conflicto entre S3c y el recierre, y casos EXTRA rotulados (no pedidos)

Con la reposicion a source, **ningun `Rrst` de los 10 k / 47 k / 100 k pedidos cumple a la vez S3c (V_GS >= 5 V a 6.8 V) y el recierre a 20 ms**: el que repone a tiempo (10 k) no enciende Q6 a 6.8-12 V, y el que mantiene el V_GS (>= 220 k) no repone en 20 ms (**inferencia**, no medido: tau = `Rrst` x C_gd >= 22 ms; el recierre con 220 k no se corrio, lo medido son 10 k/47 k/100 k/330 k en §10.5). No se ajusto nada en silencio; estos dos casos aparte exploran salidas:

- **EXTRA 1: `Rrst` 10 k con R_G bajado a 2.2 k** (`S3f_niveles_RG.log`, `S3f_rejilla_R10k_RG2k2.log`, `S3f_recierre_R10k_RG2k2.log`, `S3f_apertura*_RG2k2.log`): recupera el V_GS (S3c PASA: -5.56 V a 6.8 V/25 C, -5.52 V a 100 C; con 4.7 k -4.57 V FALLA y con 10 k -3.38 V FALLA), pero el encendido deja de ser lento: rejilla S3a **7/27** (caida hasta 9 %, PGOOD cae hasta 5 veces con 470 y 1000 uF; 25 V/100 uF u = 0.764), recierre a 5 ms 1/6 y a 20 ms 2/6. Apagado: 3.58-4.53 ms; V_DS 30.7 V a 25 V/1 mH sin diodo (25.5 V con Schottky). Descartado.
- **EXTRA 2: variante B, reposicion de `m` a la gate** (resistor `Rrstg` en paralelo con el diodo; `S3f_apertura*_B.log`, `S3f_recierreB_R*.log`): no hay efecto estatico (V_GS igual que sin remedio) y el recierre "pasa" (10 k y 47 k: 18/18), pero porque **el apagado vuelve a ser lento**: t(I<10 %) = 55.7-64.4 ms a 12 V y no termina en 100 ms a 25 V (u = 1.87-1.92, FALLA SOA a 25 V); Q6 ni abre dentro de la ventana de recierre. Equivale al remedio viejo. Descartado.

**Seguidor NPN (colector a source, emisor a gate)**: opcional, **no simulado** (queda pendiente; hay modelo NPN en `standard.bjt`, UTF-16, p. ej. BC847B).

### 10.7 Tarea 2 - convertidor idealizado mas realista (ILIM = 10 A; con y sin Rd en paralelo con Ls; BWK = 1)

Cada celda: veredicto de los tres criterios de S3a / caida / I pico de Q6 / PGOOD / reconmutaciones (reapert./recierres) / util SOA. Rd "sin" = 1 Gohm. La red "vieja" es la de §3.3 (C_gd directo = diodo puenteado con 1 mohm); la "nueva" es la de 10.1 con `Rrst` = 330 k. (`S3f_ilim_*.log`; cada uno incluye ILIM = 15 A como control.)

| V_OUT | C_carga | red nueva (diodo, Rrst 330 k), con Rd | red nueva, SIN Rd | red vieja (C gd directo = S3a_final), con Rd | red vieja, SIN Rd | ref. red nueva con Rd, ILIM 15 A |
|---|---|---|---|---|---|---|
| 7.2 V | 100 uF | PASA: caida 0.321 %, Ipk 7.97 A, PGOOD cae 0, reconm. 0/0, u 0.0617 | PASA: caida 0.324 %, Ipk 7.97 A, PGOOD cae 0, reconm. 0/0, u 0.0618 | PASA: caida 0.325 %, Ipk 7.97 A, PGOOD cae 0, reconm. 0/0, u 0.0617 | PASA: caida 0.328 %, Ipk 7.97 A, PGOOD cae 0, reconm. 0/0, u 0.0618 | caida 0.321 %, Ipk 7.97 A, u 0.0617 |
| 12 V | 100 uF | PASA: caida 0.218 %, Ipk 8.21 A, PGOOD cae 0, reconm. 0/0, u 0.113 | PASA: caida 0.303 %, Ipk 8.21 A, PGOOD cae 0, reconm. 0/0, u 0.113 | PASA: caida 0.219 %, Ipk 8.22 A, PGOOD cae 0, reconm. 0/0, u 0.112 | PASA: caida 0.306 %, Ipk 8.22 A, PGOOD cae 0, reconm. 0/0, u 0.113 | caida 0.218 %, Ipk 8.21 A, u 0.113 |
| 25 V | 100 uF | PASA: caida 0.213 %, Ipk 8.75 A, PGOOD cae 0, reconm. 0/0, u 0.576 | PASA: caida 0.367 %, Ipk 8.76 A, PGOOD cae 0, reconm. 0/0, u 0.593 | PASA: caida 0.213 %, Ipk 8.76 A, PGOOD cae 0, reconm. 0/0, u 0.576 | PASA: caida 0.368 %, Ipk 8.76 A, PGOOD cae 0, reconm. 0/0, u 0.593 | caida 0.213 %, Ipk 8.75 A, u 0.576 |
| 7.2 V | 470 uF | PASA: caida 0.358 %, Ipk 8.48 A, PGOOD cae 0, reconm. 0/0, u 0.0678 | PASA: caida 0.486 %, Ipk 8.49 A, PGOOD cae 0, reconm. 0/0, u 0.0678 | PASA: caida 0.369 %, Ipk 8.51 A, PGOOD cae 0, reconm. 0/0, u 0.0682 | PASA: caida 0.506 %, Ipk 8.51 A, PGOOD cae 0, reconm. 0/0, u 0.0681 | caida 0.358 %, Ipk 8.48 A, u 0.0678 |
| 12 V | 470 uF | PASA: caida 0.609 %, Ipk 9.39 A, PGOOD cae 0, reconm. 0/0, u 0.175 | PASA: caida 0.901 %, Ipk 9.42 A, PGOOD cae 0, reconm. 0/0, u 0.189 | PASA: caida 0.616 %, Ipk 9.42 A, PGOOD cae 0, reconm. 0/0, u 0.175 | PASA: caida 0.913 %, Ipk 9.44 A, PGOOD cae 0, reconm. 0/0, u 0.189 | caida 0.609 %, Ipk 9.39 A, u 0.175 |
| 25 V | 470 uF | PASA: caida 3.56 %, Ipk 10.7 A, PGOOD cae 0, reconm. 0/0, u 1.79 | PASA: caida 3.63 %, Ipk 10.7 A, PGOOD cae 0, reconm. 0/0, u 1.86 | PASA: caida 3.63 %, Ipk 10.7 A, PGOOD cae 0, reconm. 0/0, u 1.79 | PASA: caida 3.7 %, Ipk 10.7 A, PGOOD cae 0, reconm. 0/0, u 1.86 | caida 0.771 %, Ipk 11.7 A, u 1.79 |
| 7.2 V | 1000 uF | PASA: caida 0.677 %, Ipk 9.23 A, PGOOD cae 0, reconm. 0/0, u 0.087 | PASA: caida 0.843 %, Ipk 9.23 A, PGOOD cae 0, reconm. 0/0, u 0.0913 | PASA: caida 0.701 %, Ipk 9.29 A, PGOOD cae 0, reconm. 0/0, u 0.0882 | PASA: caida 0.878 %, Ipk 9.29 A, PGOOD cae 0, reconm. 0/0, u 0.0925 | caida 0.677 %, Ipk 9.23 A, u 0.087 |
| 12 V | 1000 uF | PASA: caida 2.08 %, Ipk 10.3 A, PGOOD cae 0, reconm. 0/0, u 0.298 | PASA: caida 2.09 %, Ipk 10.3 A, PGOOD cae 0, reconm. 0/0, u 0.308 | PASA: caida 2.22 %, Ipk 10.3 A, PGOOD cae 0, reconm. 0/0, u 0.298 | PASA: caida 2.21 %, Ipk 10.3 A, PGOOD cae 0, reconm. 0/0, u 0.306 | caida 1.05 %, Ipk 11.1 A, u 0.299 |
| 25 V | 1000 uF | FALLA: caida 9 %, Ipk 21.6 A, PGOOD cae 5, reconm. 5/5, u 3.1 | FALLA: caida 9 %, Ipk 21.6 A, PGOOD cae 5, reconm. 5/5, u 3.06 | FALLA: caida 9 %, Ipk 11.6 A, PGOOD cae 5, reconm. 0/0, u 3.11 | FALLA: caida 9 %, Ipk 11.6 A, PGOOD cae 5, reconm. 0/0, u 3.06 | caida 2.06 %, Ipk 15.4 A, u 3.14 |

- Con ILIM = 10 A **solo deja de pasar 25 V/1000 uF** (caida 9 %, PGOOD cae 5 veces; red nueva ademas reconmuta 5/5, red vieja 0/0), con o sin Rd. Con 15 A pasaba con 2.06 % (red nueva) y 2.19 % (red vieja).
- **12 V/1000 uF sigue pasando** (caida 2.08-2.22 %, I pico 10.3 A, el limite satura en 10.0 A). **25 V/470 uF** sigue pasando los tres criterios con caida 3.56-3.70 % (margen de 1.3 pts sobre el 5 %; con ILIM = 15 A era 0.77 %), pero su SOA ya fallaba (u = 1.79-1.86) con 15 A y sigue fallando.
- Quitar Rd (Ls sin amortiguar) solo sube la caida (p. ej. 25 V/100 uF 0.21 -> 0.37 %; 12 V/470 uF 0.61 -> 0.90 %) y la u de SOA unas centesimas (25 V/470 uF 1.79 -> 1.86); no cambia ningun veredicto. No se corrio BWK = 3 con ILIM = 10 A; con 15 A y BWK = 3 la caida de 25 V/470 uF era 2.58 % (§3.4), asi que el margen frente al 5 % con 10 A y fuente lenta no esta medido.

### 10.8 Tarea 3 - SOA del encendido a 25 V segun C_carga (red nueva, `Rrst` = 330 k, ILIM 15 A, BWK 1; `S3f_soa25.log`)

| C_carga [uF] | I pico Q6 [A] | caida [%] | t encendido (V_DS<0.5 V) [ms] | t con V_DS > 15 V [ms] | P pico [W] | E [mJ] | t_eq [ms] | util SOA(t_eq) | veredicto SOA |
|---|---|---|---|---|---|---|---|---|---|
| 47 | 8.33 | 0.12 | 2.77 | 1.24 | 55.8 | 111 | 1.99 | 0.456 | PASA |
| 68 | 8.5 | 0.157 | 2.77 | 1.24 | 58.6 | 117 | 2.01 | 0.502 | PASA |
| 100 | 8.75 | 0.213 | 2.77 | 1.24 | 62.8 | 127 | 2.03 | 0.576 | PASA |
| 150 | 9.16 | 0.3 | 2.77 | 1.24 | 69.9 | 143 | 2.05 | 0.717 | AL LIMITE |
| 180 | 9.4 | 0.352 | 2.77 | 1.24 | 74.2 | 152 | 2.05 | 0.809 | AL LIMITE |
| 220 | 9.72 | 0.417 | 2.77 | 1.24 | 80.3 | 165 | 2.05 | 0.96 | AL LIMITE |
| 270 | 10.1 | 0.495 | 2.77 | 1.25 | 88.2 | 180 | 2.04 | 1.15 | FALLA |
| 330 | 10.6 | 0.583 | 2.78 | 1.25 | 98.2 | 199 | 2.02 | 1.38 | FALLA |
| 470 | 11.7 | 0.771 | 2.78 | 1.25 | 134 | 242 | 1.8 | 1.79 | FALLA |

- **Tiempo con V_DS > 15 V: 1.24-1.25 ms para todo C** (47-470 uF): lo fija la red de gate (el Miller limita dV_DS/dt), no la carga; lo que crece con C es la corriente (8.3 -> 11.7 A) y la energia (111 -> 242 mJ).
- **Limite de C_carga a 25 V** (u del pulso equivalente, curvas SOA digitalizadas +-10 %, sin Zth ni temperatura): u = 0.576 (100 uF), 0.717 (150), 0.96 (220), 1.15 (270). Interpolando linealmente, u = 0.7 cae cerca de 150 uF y u = 1 cerca de 230 uF. Con una incertidumbre **supuesta** de +-30-40 % (supuesto del analista, no medido: lo unico declarado es +-10 % de digitalizacion; TC = 25 C y la falta de Zth no se cuantificaron) no se sostienen dos cifras: **limite recomendado 100-150 uF (100 uF PASA, u = 0.576; 150 uF AL LIMITE, u = 0.717), ~200 uF como cota absoluta (u = 1), >= 270 uF FALLA**; la decision final es de Luis. Coincide con `S3a_final_lim25.log` (red de C_gd directo): 150 uF 0.716, 220 uF 0.96, 330 uF 1.37.

**R de precarga en paralelo con Q6** (de VOUT_INT a VOUT_EXT; `S3f_precarga_conectada.log`, `S3f_precarga_difer.log`):

Caso (a), el modelo de S3a (carga de 8 A conectada siempre): la R de precarga forma divisor con la carga de 3.1 ohm y no precarga nada.

| C_carga [uF] | Rpre | t de precarga antes de cerrar [ms] | V_ext al cerrar [V] | V_DS al cerrar [V] | P(Rpre) pico [W] | I pico Q6 [A] | t con V_DS > 15 V [ms] | util SOA(t_eq) | veredicto SOA |
|---|---|---|---|---|---|---|---|---|---|
| 330 | ninguna | 10 | 7.91e-08 | 25 | 6.25e-07 | 10.6 | 1.25 | 1.34 | FALLA |
| 1000 | ninguna | 10 | 7.91e-08 | 25 | 6.25e-07 | 15.4 | 1.26 | 3.09 | FALLA |
| 330 | 1000 ohm | 10 | 0.0779 | 24.9 | 0.621 | 10.6 | 1.24 | 1.33 | FALLA |
| 1000 | 1000 ohm | 10 | 0.0779 | 24.9 | 0.621 | 15.4 | 1.25 | 3.06 | FALLA |
| 330 | 100 ohm | 10 | 0.758 | 24.2 | 5.88 | 10.6 | 1.17 | 1.21 | FALLA |
| 1000 | 100 ohm | 10 | 0.758 | 24.2 | 5.88 | 15.4 | 1.18 | 2.73 | FALLA |
| 330 | ninguna | 100 | 7.91e-08 | 25 | 6.25e-07 | 10.6 | 1.25 | 1.34 | FALLA |
| 1000 | ninguna | 100 | 7.91e-08 | 25 | 6.25e-07 | 15.4 | 1.26 | 3.09 | FALLA |
| 330 | 1000 ohm | 100 | 0.0779 | 24.9 | 0.621 | 10.6 | 1.24 | 1.33 | FALLA |
| 1000 | 1000 ohm | 100 | 0.0779 | 24.9 | 0.621 | 15.4 | 1.25 | 3.06 | FALLA |
| 330 | 100 ohm | 100 | 0.758 | 24.2 | 5.88 | 10.6 | 1.17 | 1.21 | FALLA |
| 1000 | 100 ohm | 100 | 0.758 | 24.2 | 5.88 | 15.4 | 1.18 | 2.73 | FALLA |

(El `.op` precarga C_carga por Rpre hasta el equilibrio; por eso 10 y 100 ms dan lo mismo.) **No sirve**: V_DS al cerrar 24.2-24.9 V, u 1.21-3.06 (FALLA) aun con 100 ohm (5.9 W).

Caso (b), EXTRA rotulado: carga de 8 A conectada 30 ms **despues** del cierre; C_carga descargada con `.ic` en t = 0 y VOUT_INT ya a 25 V (el tiempo de precarga es el que pasa hasta cerrar Q6; la u mide solo el encendido, antes de aplicar la carga):

| C_carga [uF] | Rpre | t de precarga antes de cerrar [ms] | V_ext al cerrar [V] | V_DS al cerrar [V] | P(Rpre) pico [W] | I pico Q6 [A] | t con V_DS > 15 V [ms] | util SOA(t_eq) | veredicto SOA |
|---|---|---|---|---|---|---|---|---|---|
| 330 | ninguna | 10 | 7.67e-07 | 25 | 6.25e-07 | 3.63 | 1.24 | 0.899 | AL LIMITE |
| 1000 | ninguna | 10 | 2.53e-07 | 25 | 6.25e-07 | 11.8 | 1.26 | 2.56 | FALLA |
| 330 | 4700 ohm | 10 | 0.161 | 24.8 | 0.133 | 3.63 | 1.24 | 0.873 | AL LIMITE |
| 1000 | 4700 ohm | 10 | 0.0532 | 24.9 | 0.133 | 11.8 | 1.25 | 2.54 | FALLA |
| 330 | 1000 ohm | 10 | 0.746 | 24.3 | 0.625 | 3.61 | 1.21 | 0.784 | AL LIMITE |
| 1000 | 1000 ohm | 10 | 0.249 | 24.8 | 0.625 | 11.8 | 1.25 | 2.45 | FALLA |
| 330 | 470 ohm | 10 | 1.56 | 23.4 | 1.33 | 3.58 | 1.14 | 0.671 | PASA |
| 1000 | 470 ohm | 10 | 0.527 | 24.5 | 1.33 | 11.8 | 1.24 | 2.32 | FALLA |
| 330 | 100 ohm | 10 | 6.52 | 18.5 | 6.25 | 3.44 | 0.647 | 0.24 | PASA |
| 1000 | 100 ohm | 10 | 2.38 | 22.6 | 6.25 | 11.6 | 1.08 | 1.58 | FALLA |
| 330 | ninguna | 100 | 7.67e-06 | 25 | 6.25e-07 | 3.63 | 1.24 | 0.899 | AL LIMITE |
| 1000 | ninguna | 100 | 2.53e-06 | 25 | 6.25e-07 | 11.8 | 1.26 | 2.56 | FALLA |
| 330 | 4700 ohm | 100 | 1.56 | 23.4 | 0.133 | 3.63 | 1.11 | 0.683 | PASA |
| 1000 | 4700 ohm | 100 | 0.526 | 24.5 | 0.133 | 11.8 | 1.22 | 2.33 | FALLA |
| 330 | 1000 ohm | 100 | 6.42 | 18.6 | 0.625 | 3.61 | 0.639 | 0.262 | PASA |
| 1000 | 1000 ohm | 100 | 2.38 | 22.6 | 0.625 | 11.8 | 1.05 | 1.63 | FALLA |
| 330 | 470 ohm | 100 | 11.6 | 13.4 | 1.33 | 3.6 | 0 | 0.0897 | PASA |
| 1000 | 470 ohm | 100 | 4.77 | 20.2 | 1.33 | 11.8 | 0.816 | 1.01 | FALLA |
| 330 | 100 ohm | 100 | 23.4 | 1.6 | 6.25 | 3.08 | 0 | 0.0241 | PASA |
| 1000 | 100 ohm | 100 | 15.7 | 9.31 | 6.25 | 11.6 | 0 | 0.103 | PASA |
| 330 | ninguna | 300 | 2.3e-05 | 25 | 6.25e-07 | 3.63 | 1.24 | 0.899 | AL LIMITE |
| 1000 | ninguna | 300 | 7.59e-06 | 25 | 6.25e-07 | 11.8 | 1.26 | 2.56 | FALLA |
| 330 | 4700 ohm | 300 | 4.23 | 20.8 | 0.133 | 3.63 | 0.834 | 0.392 | PASA |
| 1000 | 4700 ohm | 300 | 1.55 | 23.5 | 0.133 | 11.8 | 1.11 | 1.93 | FALLA |
| 330 | 1000 ohm | 300 | 14 | 11 | 0.625 | 3.61 | 0 | 0.0499 | PASA |
| 1000 | 1000 ohm | 300 | 6.37 | 18.6 | 0.625 | 11.8 | 0.637 | 0.766 | AL LIMITE |
| 330 | 470 ohm | 300 | 20.2 | 4.84 | 1.33 | 3.57 | 0 | 0.0218 | PASA |
| 1000 | 470 ohm | 300 | 11.5 | 13.5 | 1.33 | 11.7 | 0 | 0.257 | PASA |
| 330 | 100 ohm | 300 | 24.5 | 0.486 | 6.25 | 2.26 | 0 | 0 | PASA |
| 1000 | 100 ohm | 300 | 23.4 | 1.63 | 6.25 | 9.4 | 0 | 0.0672 | PASA |

- Sin Rpre el encendido sin carga da u = 0.899 (330 uF, AL LIMITE) y 2.56 (1000 uF, FALLA). Con **330 uF** basta Rpre <= 470 ohm con 10 ms de precarga, <= 4.7 k con 100 ms. Con **1000 uF** solo 100 ohm con 100 ms (u = 0.103, 6.25 W) o <= 470 ohm con 300 ms (u = 0.257, 1.33 W); 1 k con 300 ms queda en 0.766 (AL LIMITE).
- Costo: P(Rpre) pico = V^2/R = 0.133 W (4.7 k), 0.625 W (1 k), 1.33 W (470 ohm), 6.25 W (100 ohm); con Q6 abierto y sin carga VOUT_EXT sigue a VOUT_INT, **se pierde el aislamiento de la salida** (LED3 siempre encendido, la carga ve la tension). Y el PGOOD llega al final del soft-start (1.845 ms en S4a): los 100-300 ms exigen un retardo extra en el cierre de Q6 que el circuito no tiene. Con el tiempo realista (~2-10 ms) la precarga solo ayuda a 330 uF con <= 470 ohm (1.3-6.3 W) y nunca a 1000 uF.

### 10.9 Pasa/falla (S3f) y decisiones que quedan

| Criterio | Medido | Veredicto |
|---|---|---|
| Apagado <= ~8 ms (25 V/8 A) | 7.94-8.19 ms (`Rrst` >= 47 k), 3.7-3.9 ms (10 k) | PASA |
| SOA del apagado u < 0.7 (25 V, 0.5 uH / 1 mH) | 0.205-0.22 / 0.697-0.716 sin D, 0.647-0.66 con D | PASA con 0.5 uH; AL LIMITE con 1 mH sin diodo |
| V_DS < 32 V (25 V/1 mH) | 30.7 V sin diodo, 25.5 V con Schottky; u 0.716 sin D, 0.66 con D | V_DS AL LIMITE sin diodo (30.7 V > 25.6 V; margen 1.3 V) y u > 0.7: diodo AL LIMITE, recomendado para L >= 1 mH a 25 V; con Schottky V_DS 25.5 V PASA (justo bajo 25.6 V) |
| Rejilla S3a (caida/PGOOD/reconm.) | 27/27 con 47 k, 100 k, 330 k; 10 k: 18/27 validos (9 de 7.2 V sin cierre de Q6); caida max 4.36 % (BWK 3, 330 k) | PASA 47 k/100 k/330 k; 10 k invalido a 7.2 V |
| S3c V_GS >= 5 V a 6.8 V | 10 k -2.31 V, 47 k -4.03 V, 100 k -4.67 V, 330 k -5.19 V (100 C) | FALLA 10 k/47 k/100 k; PASA >= 220 k |
| Recierre a 1 ms | 6/6 con todas las `Rrst` | PASA |
| Recierre a 5 ms | 10 k 3/6, 47 k 5/6, 100 k y 330 k 6/6 | FALLA 10 k y 47 k |
| Recierre a 20 ms | 10 k 6/6, 47 k 5/6, 100 k y 330 k 0/6 (control sin reposicion 0/6) | FALLA 47 k/100 k/330 k; 10 k incompatible con S3c |
| Recierre con 330 k a >= 200 ms (solo 12/25 V x 470/1000 uF, 4 casos por GAP) | 4/4 y V(m)-V(vint) <= 0.11 V; a 100 ms 3/4 (falla 25 V/470 uF); a 50 ms 0/4 | PASA solo con GAP >= 200 ms |
| SOA del encendido a 25 V | 470 uF u = 1.76-1.79; 1000 uF u = 3.1; 150-220 uF u = 0.72-0.96 | FALLA >= 270 uF; AL LIMITE 150-220 uF (igual que antes del diodo) |
| ILIM = 10 A (con y sin Rd) | solo 25 V/1000 uF falla (caida 9 %); 12 V/1000 uF 2.08 %, 25 V/470 uF 3.56 % | FALLA 25 V/1000 uF |
| Precarga de Rpre | carga siempre conectada: u >= 1.21; carga diferida: solo con >= 100 ms o <= 470 ohm | NO SIRVE como esta |

Decisiones para Luis (no se tomaron aqui):
1. **Con el diodo en serie y `Rrst` >= 330 k** se cumple apagado de 8 ms, S3a, S3b y S3c, pero queda sin encendido lento un recierre antes de ~200 ms (a 100 ms todavia falla 25 V/470 uF). Hay que garantizar un retardo de recierre >= 200 ms o aceptar el riesgo; el hiccup del LM5175 no es simulable aqui. Alternativa: mantener C_gd directo 100 nF + R_G 22 k (§8.1), que no tiene problema de recierre pero apaga en 65 ms / > 100 ms (SOA de apagado 25 V/8 A u = 1.88, FALLA). Como PGOOD tambien abre Q6, esa apertura a 25 V/8 A ocurre en operacion normal, lo que favorece el apagado rapido.
2. Diodo de rueda libre: **AL LIMITE, recomendado (u 0.716 vs 0.7)** si hay cargas >= ~1 mH a 25 V con apagado rapido; sin diodo no FALLA pero queda sin margen; no hace falta para 0.5 uH. Montarlo o DNP: decision de Luis.
3. Limite de C_carga a 25 V: limite recomendado 100-150 uF (150 uF AL LIMITE, u = 0.717); ~200 uF como cota; la decision final es de Luis; mas exige cambiar de FET o un retardo de precarga de cientos de ms (no existente).
4. Con ILIM real ~10 A, 25 V/1000 uF ya no pasa ni siquiera los criterios electricos.

**Conclusion de §10 (lo que NO queda resuelto)**:
- **S3b con 1 mH sin diodo queda AL LIMITE, no PASA**: u 0.716 vs 0.7 y V_DS 30.7 V (> 25.6 V). Con Schottky: u 0.66, V_DS 25.5 V (PASA, justo bajo el umbral).
- **La SOA del encendido a 25 V con 470 uF FALLA** (u 1.76-1.79; 1.79-1.86 con ILIM 10 A) y con 1000 uF u ~ 3.1; el nuevo montaje de gate no la mejora respecto de §3.3. Solo <= 100 uF pasa, 150-220 uF queda AL LIMITE.
- **El margen de V_GS a 6.8 V/100 C es 0.19 V** (330 k: -5.19 V contra el minimo de 5 V) y es solo nominal: **no incluye tolerancias de R** (`Rrst`, R_G, 100 k G-S) **ni del zener**, que no estan modeladas; con 220 k el margen es 0.07 V.

## 11. Archivos
`common_S3.inc` (circuito), `soa_S3.inc` (SOA), `S3a_*.cir/.log` (base, R1, R1x, R2, R3, R3b, R12, final, final_lim25), `S3b_apertura*.cir/.log` (sin/con diodo, sin/con remedio), `S3c_niveles*.cir/.log` (sin/con remedio), `S3d_bleeder`, `S3e_descarga_CIN`, `run.sh` (ejecucion secuencial con reintento; LTSPICE_EXE apunta a una copia privada de LTspice.exe), `s3_post.py` (tablas/CSV), `csv/` (<= 5000 puntos: peor S3a base 25 V/1000 uF, R1 100 nF 25 V/1000 uF, final 25 V/470 y 1000 uF, S3b 25 V/1 mH sin remedio y con 100 nF).

**S3f (seguimiento, §10)**: `S3f_common.inc` (circuito con diodo + `Rrst`; copia de `common_S3.inc`), `S3f_niveles(.cir/.log)`, `S3f_niveles_ext`, `S3f_niveles_RG` (S3c), `S3f_apertura`, `S3f_apertura_dfw` (S3b sin/con Schottky), `S3f_rejilla_R{10k,47k,100k,330k}` (rejilla S3a), `S3f_recierre_R{1G,10k,47k,100k,330k}`, `S3f_recierre_R330k_largo`, `S3f_ilim_{nuevo,viejo}_{Rd,sinRd}` (ILIM 10 A), `S3f_soa25`, `S3f_precarga_conectada`, `S3f_precarga_difer`; casos EXTRA: `S3f_*_RG2k2`, `S3f_apertura*_B`, `S3f_recierreB_R*`. Scripts: `S3f_post.py`, `S3f_tablas.py` (tablas desde los `.log`), `S3f_gen_recierre.py`, `S3f_gen_ilim_soa.py`, `S3f_gen_precarga.py` (generan los `.cir`), `S3f_run2.sh`, `S3f_show_ilim.py`, `S3f_show_precarga.py`. CSV en `csv/`: `S3f_apertura_25V_1mH_R47k.csv`, `S3f_recierre_R47k_25V_470uF_gap20ms.csv`, `S3f_recierre_R330k_25V_470uF_gap20ms.csv`, `S3f_recierre_R10k_25V_470uF_gap5ms.csv`. Los `.raw`/`.db` de S3f (~0.7-0.9 GB por caso) se borraron; se regeneran corriendo el `.cir`.
