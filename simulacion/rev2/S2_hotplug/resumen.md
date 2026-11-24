# S2 - Conexion en caliente + fusible F1 (inrush), Rev. 2 (corregido)

Sin LM5175. LTspice 26.0.2, `.options solver=alt method=modtrap`. Todo numero sale de un `.meas` de los `.log` de esta carpeta (corridos el 16-nov-2026, despues de fijar los `.cir`; todos los logs terminan en "Total elapsed time" salvo los que se declaran como no convergentes). Ningun `.log` valido tiene avisos "tolerance relaxed".
Criterio de estado: FALLA si no cumple; AL LIMITE si cumple con margen < 20 % del limite; PASA si no.

## Circuito simulado
BAT (fuente ideal DC) + R_bat + L_cable -> interruptor (Ron 100 uohm, cierra en t = 1 ms, flanco 1 ns, sin rebote) -> F1 (1.92 mohm, Littelfuse 0456025.ER) -> Q5 SiRS4401DP (drain a bateria, source a VBUS; 47 kohm gate-GND; zener 12 V gate-source, catodo en source) -> VBUS con C_IN = 100 uF polimero (ESR 31 mohm) + 10 uF (3 mohm) + 100 nF (10 mohm) y LED1 + 4.7 kohm. Sin carga de convertidor (peor caso de tension). Ventana de medida 1-6 ms.
Modelo Q5: `..\modelos\SiRS4401DP_PS.TXT` (Vishay). Zener y LED de `standard.dio` de LTspice (no son el MM3Z12VT1G ni el LED real).
ESR del polimero: KEMET A759KS107M1V E031 = 31 mohm max a 100 kHz (identificado en `Datasheets\KEMET_A4072_A759_condensadores.pdf`); se hizo sensibilidad 15/60 mohm (S2e) porque 31 mohm es un maximo.
Limite I2t: 25 % de 45 A2s = 11.25 A2s (Littelfuse 456 NANO2). Limite de corriente de pulso de Q5: I_DM = 350 A a 100 us (`modelos\SiRS4401DP_datasheet.pdf`).

## Modelo del zener: Cjo = 1p en TODOS los .cir de S2 (desviacion declarada)
`standard.dio` trae `BZX84C12L ... Cjo=150p`. En todos los `.cir` de S2 se usa el mismo modelo con **`Cjo=1p`** (unica diferencia, ver comentario en cada `.cir`; el zener aporta 1.6 % de Cgs = 9 nF, despreciable). Motivo (evidencia en esta carpeta, `ev150/` y `celdas_Cjo150p/`, `S2_evidencia_Cjo150p.*`):
- Con `Cjo=150p`: el barrido de 12 V (6 casos) y `12V_30m` (2 casos) convergen (`ev150\S2a_hotplug_12V_c150.log`, `..._12V_30m_c150.log`, terminan en "Total elapsed"), pero el barrido de 25.2 V **no completa**: se queda en el paso 4 (R_bat 10 mohm, L 0.5 uH) mas de 280 s sin avanzar (`ev150\S2a_hotplug_25V_c150.log`, corrida cortada por mi). Los casos de remedio tampoco convergen en 40 s (`celdas_Cjo150p\`: S2c N=1 con 978 avisos "relaxed"; S2b 0 de 7 celdas probadas).
- Con `Cjo=1p`: 17 de los 18 casos del barrido principal convergen desde C_IN = 0 V (ver abajo).
Ensayos que no cambiaron la no-convergencia en el cierre (S2c N=2, a 1.1 ms): Cjo = 0 / 10p / 150p, quitar zener, quitar LED, otro modelo de LED, `gmin=1e-9`, `trtol=3`, solver normal/trap, gear, paso maximo 0.05/1 us, flanco del interruptor 10/100 ns, Ron 1 mohm, bleeder 10k/100k. Lo unico que lo arregla es precargar C_IN (siguiente seccion).
Nota: S1 (`..\S1_polaridad`) y S2 comparten el mismo modelo de zener (`BZX84C12L`, `Cjo=1p`); el valor de la libreria (`standard.dio`) es 150p.

## Condicion inicial de C_IN
- Barrido principal (S2a): C_IN inicia en 0 V (`.ic V(vbus)=0`), el peor caso de tension.
- Remedios (S2b, S2c, S2d): con C_IN exactamente en 0 V LTspice **no converge** en el cierre para casi todas las celdas con capacidad extra (corrida celda a celda con 0 V, 60 s por celda, ver `celdas_old1p\`: S2c 1 de 3 celdas convergen, S2b 9 de 20, S2d 0 de las 10 celdas probadas, corrida cortada). Se usa `.ic V(vbus)=1.0` (orden de la tension de fuga del interruptor abierto por el LED, ~1.0-1.3 V; probado en S2c N=2: con 0.05/0.2/0.5 V no converge, con 1.0 V si). La precarga **baja** el pico: comparacion pareada en los 9 casos de 25.2 V (`S2a_hotplug_25V.log` a 0 V contra `S2a_hotplug_25V_precarga1V.log`): -0.06 a -0.54 V (p.ej. 37.587 -> 37.049 V). Por tanto los numeros de remedio son hasta ~0.5 V optimistas frente a 0 V; la referencia sin remedio para compararlos es `S2a_hotplug_25V_precarga1V.log`.

## Barrido principal S2a: 18 casos (R_bat 10/20/30 mohm x L_cable 0.2/0.5/1 uH x BAT 12.6/25.2 V)
Archivos (el barrido se corre en tres `.cir` porque un caso no converge; juntos son los 18): `S2a_hotplug_12V.log` (6 casos, 10/20 mohm), `S2a_hotplug_12V_30m.log` (2 casos, 30 mohm con 0.5/1 uH), `S2a_hotplug_25V.log` (9 casos), y el caso 12.6 V / 30 mohm / 0.2 uH aparte (abajo). Las 3 corridas terminan en "Total elapsed time" (3.0, 1.0 y 5.9 s), sin avisos.

Criterio V(VBUS) pico < 32 V (margen = (32 - V)/32):

| R_bat \ L_cable | 0.2 uH | 0.5 uH | 1 uH |
|---|---|---|---|
| **25.2 V**, 10 mohm | 30.52 V AL LIMITE (4.6 %) | 34.67 V FALLA | 37.59 V FALLA |
| 25.2 V, 20 mohm | 27.86 V AL LIMITE (12.9 %) | 32.21 V FALLA (0.7 % sobre) | 35.43 V FALLA |
| 25.2 V, 30 mohm | 25.98 V AL LIMITE (18.8 %) | 30.18 V AL LIMITE (5.7 %) | 33.56 V FALLA |
| **12.6 V**, 10 mohm | 14.79 V PASA | 16.80 V PASA | 18.22 V PASA (peor, margen 43 %) |
| 12.6 V, 20 mohm | 13.50 V PASA | 15.61 V PASA | 17.18 V PASA |
| 12.6 V, 30 mohm | ver abajo | 14.63 V PASA | 16.27 V PASA |

Resumen: a 25.2 V **5 FALLA y 4 AL LIMITE (0 PASA)**; a 12.6 V todo PASA (8 casos medidos desde 0 V). Con 25.2 V el regimen ya consume 21 % del limite (25.2/32), por eso casi todo cae en AL LIMITE. El pico crece con L_cable y baja con R_bat. Respecto del resumen anterior: ahora FALLAN 5 casos (antes 4); el caso 25.2 V / 20 mohm / 0.5 uH pasa de 31.80 V (AL LIMITE) a 32.21 V (FALLA).

Caso 12.6 V / 30 mohm / 0.2 uH (el 18): **no converge con C_IN en 0 V** (`S2a_hotplug_12V_30m_0u2_precarga.log`, version con rampa de BAT, cortado a los 200 s en t = 1.0665 ms con miles de avisos "relaxed"; el mismo caso con `.ic 0` tambien se corto, 3585 avisos). Con precarga 1.0 V converge (`S2a_hotplug_12V_30m_0u2_precarga1V.log`, 0.5 s): pico V(VBUS) = 12.60 V (sin sobreimpulso medible), pico de corriente 118.2 A, I2t 0.109 A2s, |V_GS| 11.51 V. Estado: PASA, con la salvedad de que es con precarga y de que 12.60 V no sigue la tendencia de 10/20 mohm (14.79/13.50 V): tratar como informativo.

Otros criterios (los 17 casos desde 0 V + el caso 18 con precarga):

| Criterio | Medido | Estado |
|---|---|---|
| I2t del inrush en F1 (INTEG I(F1)^2, 1-6 ms) < 11.25 A2s | 25.2 V: 0.50 .. 0.81 A2s (max 10 mohm, 0.2 uH); 12.6 V: 0.109 .. 0.190 (min = caso 18 con precarga, 0.109; desde 0 V: 0.115 .. 0.190) | PASA (max = 7.2 % del limite) |
| \|V_GS\| Q5 <= 15 V | 25.2 V: 11.84 V max (min V_GS = -11.837); 12.6 V: 11.51 V; max V_GS positivo = +0.30 V | PASA (margen 21.1 %, justo sobre el umbral de 20 %) |
| V_DS inverso de Q5 (V(VBUS) - V(drain)) < 32 V (regla comun; los 40 V son el maximo absoluto) | max 25.68 V (25.2 V, 10 mohm, 1 uH) y 25.62 V (10 mohm, 0.5 uH); resto de 25.2 V: <= 24.90 V; 12.6 V: max 19.09 V | 25.68 V y 25.62 V: AL LIMITE (margen 19.8 % y 19.9 %, < 20 %); los otros 7 casos de 25.2 V y los 9 de 12.6 V: PASA |
| V_DS directo (drain-source, diodo) | 0.82 .. 1.00 V | informativo |
| Pico de corriente por F1/Q5 contra I_DM = 350 A (100 us) | 25.2 V: 170 .. **328.9 A** (max: 10 mohm, 0.2 uH); 12.6 V: 82.6 .. 159.3 A | 328.9 A: AL LIMITE (6.0 % bajo 350 A) para el peor caso sin remedio; el resto PASA |

El I_DM de 350 A es limite de pulso a 100 us de la hoja de datos; la curva SOA del SiRS4401DP no se leyo (texto del PDF ilegible), asi que la comparacion es solo contra I_DM.

Mecanismo: la corriente (hasta ~330 A) rebota entre L_cable y el ceramico de 10 uF (Z0 = sqrt(L/C) ~ 0.14-0.32 ohm); el polimero lo amortigua solo a traves de su ESR. De ahi la sensibilidad a la ESR (S2e).

## Sensibilidad a la ESR del polimero (`S2e_esr.log`, 25.2 V, sin remedio, C_IN desde 0 V, 12 casos completos)
Pico V(VBUS) [V]:

| R_bat | L_cable | ESR 15 mohm | ESR 31 mohm | ESR 60 mohm |
|---|---|---|---|---|
| 10 mohm | 0.5 uH | 37.83 F | 34.67 F | 31.10 AL LIMITE (2.8 %) |
| 20 mohm | 0.5 uH | 34.83 F | 32.21 F | 29.20 AL LIMITE (8.8 %) |
| 10 mohm | 1 uH | 40.41 F | 37.59 F | 34.02 F |
| 20 mohm | 1 uH | 37.89 F | 35.43 F | 32.30 F |

V_DS inverso de Q5 [V] (`vds_rev` de `S2e_esr.log`; limite 32 V, AL LIMITE si > 25.6 V):

| R_bat | L_cable | ESR 15 mohm | ESR 31 mohm | ESR 60 mohm |
|---|---|---|---|---|
| 10 mohm | 0.5 uH | 27.21 AL LIMITE (15.0 %) | 25.62 AL LIMITE (19.9 %) | 15.32 PASA |
| 20 mohm | 0.5 uH | 25.68 AL LIMITE (19.8 %) | 22.78 PASA | 9.78 PASA |
| 10 mohm | 1 uH | 26.41 AL LIMITE (17.5 %) | 25.68 AL LIMITE (19.8 %) | 23.56 PASA |
| 20 mohm | 1 uH | 25.91 AL LIMITE (19.0 %) | 24.90 PASA | 20.76 PASA |

Los valores con ESR 31 mohm coinciden con S2a (mismos casos). Una ESR menor que el maximo de 31 mohm (lo habitual en la practica) empeora el pico: 40.41 V en el peor caso. Con ESR 60 mohm ningun caso pasa con holgura.

## Remedios (casos aparte; peor esquina: 25.2 V, R_bat 10 mohm, L_cable 1 uH, ESR 31 mohm; C_IN inicia en 1.0 V). Sin remedio en esas condiciones: **37.05 V** (`S2a_hotplug_25V_precarga1V.log`), I2t 0.652 A2s, pico 185.0 A.

### A. Snubber RC serie VBUS-GND (`S2b_snubber.log`, 20 celdas, "Total elapsed" 7.3 s): V(VBUS) pico [V] / I2t de F1 [A2s] / pico I snubber [A]

| C_sn \ R_sn | 0.05 ohm | 0.10 ohm | 0.22 ohm | 0.47 ohm |
|---|---|---|---|---|
| 47 uF | 37.41 / 1.19 / 68 | 36.47 / 1.07 / 63 | 35.27 / 0.89 / 53 | 34.93 / 0.75 / 39 |
| 100 uF | 35.99 / 1.60 / 116 | 34.25 / 1.27 / 101 | 33.19 / 0.95 / 77 | 33.93 / 0.76 / 50 |
| 220 uF | 32.89 / 2.26 / 177 | 30.89 / 1.58 / 143 | 31.28 / 1.07 / 97 | 33.27 / 0.82 / 58 |
| 470 uF | 29.12 / 3.41 / 234 | **27.88 / 2.16 / 176** | 30.14 / 1.34 / 111 | 32.94 / 0.97 / 63 |
| 1000 uF | 25.73 / 5.72 / 282 | 25.88 / 3.46 / 199 | 29.53 / 1.98 / 119 | 32.77 / 1.28 / 65 |

Baja de 32 V solo con C_sn >= 220 uF y R_sn 0.10-0.22 ohm, o C_sn >= 470 uF con 0.05-0.22 ohm (1000 uF tambien con 0.05-0.22). Un snubber de 47-100 uF no basta. Todas las celdas tienen |V_GS| max 11.84 V.

### B. Mas capacidad con baja ESR (`S2c_capextra.log`, 3 casos): N x 100 uF de polimero extra (ESR 31 mohm / N)
N = 1: 37.00 V; N = 2: 36.71 V; N = 4: 35.99 V (I2t 1.79 / 3.21 / 6.48 A2s; pico 256.6 / 309.8 / 389.3 A). **No resuelve** (sigue FALLA) y sube la corriente (con N = 4 el pico supera I_DM = 350 A): lo que amortigua es resistencia, no capacidad con ESR baja.

### C. Remedio propuesto: snubber 470 uF + 0.10 ohm en VBUS (`S2d_remedio_grid.log` y `S2d_remedio_grid_12V.log`)
18 casos a 25.2 V (9 esquinas R_bat x L_cable x ESR del polimero 15 y 31 mohm) y 18 a 12.6 V; ambos terminan en "Total elapsed" (6.9 y 6.4 s), sin avisos. C_IN inicia en 1.0 V.

| Medida | Resultado | Estado |
|---|---|---|
| Pico V(VBUS) a 25.2 V | max 28.29 V (R_bat 10 mohm, L 1 uH, ESR 15 mohm; 27.88 V con ESR 31 mohm). 11 de 18 casos sin sobreimpulso (25.20 V), 7 entre 25.62 y 28.29 V | cumple < 32 V en los 18; peor caso AL LIMITE (margen 11.6 %); 7 casos AL LIMITE, 11 PASA. Hasta ~0.5 V optimista por la precarga |
| Pico V(VBUS) a 12.6 V | max 13.62 V | PASA (margen 57 %) |
| I2t de F1 | max 2.49 A2s (25.2 V, 10 mohm, 0.2 uH, ESR 15 mohm); 12.6 V: max 0.53 | PASA (22.1 % del limite 11.25) |
| \|V_GS\| Q5 | 11.84 V (25.2 V), 11.51 V (12.6 V) | PASA (margen 21.1 %) |
| Corriente en el snubber | pico max 214.1 A, I2t max 1.27 A2s (25.2 V) | informativo |
| **Pico de corriente por F1/Q5 vs I_DM = 350 A (100 us)** | max **422.4 A** (25.2 V, 10 mohm, 0.2 uH, ESR 15 mohm); tambien 387.8 A (ESR 31 mohm, mismo caso) y 365.2 A (20 mohm, 0.2 uH, ESR 15 mohm); sin snubber ese caso era 315.4 A con la misma precarga de 1.0 V y ESR 31 mohm (`S2a_hotplug_25V_precarga1V.log`; 328.9 A desde 0 V) | **FALLA vs I_DM en 3 de 18 casos a 25.2 V** (a 12.6 V el max es 195.3 A, PASA) |

Conclusion del remedio: el snubber 470 uF + 0.10 ohm **corrige la tension** (de FALLA 37.05 V, referencia con precarga 1.0 V y ESR 31 mohm en `S2a_hotplug_25V_precarga1V.log` [37.59 V desde 0 V], a <= 28.29 V con ESR 15 mohm / 27.88 V con ESR 31 mohm, AL LIMITE) pero **sube el pico de corriente de Q5 por encima de I_DM = 350 A** (hasta 422 A, duracion de microsegundos). Por eso el remedio queda **pendiente de verificar contra la SOA de Q5 (a 10-100 us)**; no se declara resuelto. Alternativas a evaluar: mas R_sn (0.22 ohm: 30.14 V en la peor esquina, pico 111 A en el snubber y menor corriente de pico por Q5, pero menos margen de tension) o limitar la corriente de cierre en otro lado. No se modela la ESL ni el limite de ripple/pico del capacitor snubber; no se eligio pieza concreta (sin datos de fabricante en `modelos/`). Si la ESR real del polimero fuera menor que 15 mohm no se simulo.

## Supuestos y limitaciones
- Sin carga conectada a VBUS (convertidor apagado/UVLO). Sin rebote de contacto del XT60 (ideal, 1 ns): un cierre real con rebote puede producir varios picos.
- R_bat 10-30 mohm y L_cable 0.2-1 uH son los valores del barrido pedido (no medidos). Sin DC-bias derating de los ceramicos (a 25 V un ceramico pierde buena parte de sus 10 uF; cambiaria el pico, no se simulo). Sin ESL de capacitores.
- F1 = resistencia constante 1.92 mohm (en frio). La fusion no se simula; solo se compara I2t (`INTEG I(Vid)*I(Vid)`, 1-6 ms) contra 25 % de 45 A2s.
- Modelo Vishay de Q5 nominal (sin dispersion); zener/LED de `standard.dio` (zener con Cjo = 1p, ver arriba). Arranque con rampa de BAT: ver `..\S1_polaridad\resumen.md` (en este banco solo se probo en el caso 18, que no converge desde 0 V; arriba).
- La no-convergencia del cierre es sensible a detalles numericos (condicion inicial, Cjo, valores de la rejilla); los casos que no convergen se reportan como tales, no se ajustaron en silencio. Un cambio de modelo (otro MOSFET/diodo) puede mover cuales casos convergen.

## Archivos
- Resultados: `S2a_hotplug_12V`, `S2a_hotplug_12V_30m`, `S2a_hotplug_25V`, `S2a_hotplug_12V_30m_0u2_precarga1V` (caso 18), `S2a_hotplug_25V_precarga1V` (referencia a 1.0 V), `S2b_snubber`, `S2c_capextra`, `S2d_remedio_grid`, `S2d_remedio_grid_12V`, `S2e_esr` (`.cir` + `.log`).
- No convergente (se deja como evidencia): `S2a_hotplug_12V_30m_0u2_precarga.cir/.log` (rampa de BAT, cortado a los 200 s).
- Evidencia del modelo: `S2_evidencia_Cjo150p.*`, `ev150\` (Cjo = 150p, barridos de S2a y diagnosticos), `celdas_Cjo150p\` y `celdas_old1p\` (corridas celda a celda de los remedios con 150p y con 1p desde 0 V), `celdas\` (celda a celda con precarga, redundante con los `.log` principales; la celda 0.05/1000u de ese directorio fallo por un error de lectura del archivo, no por convergencia; el barrido principal S2b la contiene), `run_celdas.py`, `leer_celdas.py`.
- `csv/`: S2a_hotplug_25V (los 9 casos, todos FALLA o AL LIMITE) y S2d (peor caso con remedio), remuestreados a 5000 pts en 0.95-2.5 ms; picos verificados contra `.meas` (37.587 V y 28.288 V). `raw2csv.py` regenera los csv desde los `.raw`.
