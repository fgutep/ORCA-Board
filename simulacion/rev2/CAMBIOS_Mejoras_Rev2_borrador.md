# Lista de cambios de hardware Rev. 2 (LM5175) - BORRADOR

Insumo para `PCB_Potencia_Mejoras_Rev2.md`. Fuentes: `S1_polaridad/resumen.md`, `S2_hotplug/resumen.md`, `S3_salida/resumen.md` (§8 y §10), `S5_escalon/resumen.md`, `ESTADO_PAUSA.md`, `HANDOFF_Proximo_Chat.md` §4, `README_rev2.md`. Todo numero sale de esos resumenes (que a su vez citan `.meas`); lo marcado "(aritmetica)" es cuenta directa sobre ellos.

Notas de alcance:
- `PCB_Potencia_Mejoras_Rev2.md` no esta en el repo (el handoff lo ubica en `C:\Users\Lenovo\Downloads\`). Los "valores actuales" salen de los resumenes de simulacion; lo que no aparece ahi va como "pendiente de verificar contra el esquematico".
- Numeros de catalogo: solo los de `README_rev2.md`. Cualquier otra pieza = "pendiente de verificar". Preferencia SMD; electroliticos/polimero pueden ser THT (regla de Luis).
- Estados: **APROBADO** (aprobado por simulacion), **DECISION** (pendiente de decision de Luis), **SOA** (pendiente de verificar contra la SOA de la pieza).
- S4b-e (convertidor completo con Q5/Q6 y eventos) aun no corrio. **S4c (cerrar Q6 contra 8 A + 470 uF, con y sin remedio) es el arbitro real de C1**; mientras no corra, C1 no se cierra.

## Resumen de estados

| # | Cambio | Estado |
|---|---|---|
| C1 | Q6: red de gate (C_gd, R_G, diodo, Rrst) | DECISION |
| C2 | Diodo de rueda libre en VOUT_EXT | DECISION |
| C3 | Limite de C_carga a 25 V (o cambio de FET) | DECISION |
| C4 | LED1/LED2/LED3: resistencias | APROBADO (0805 vs >= 6.8 k: elige Luis) |
| C5 | Bleeder 1.5 k, 2512 de 1 W | APROBADO (sin cambio) |
| C6 | Q5 = SiRS4401DP (proteccion de polaridad) | APROBADO |
| C7 | F1 = 0456025.ER | APROBADO (sin cambio) |
| C8 | Snubber RC en VBUS (hot-plug a 25.2 V) | SOA + DECISION |
| C9 | Polaridad invertida con C_IN cargado | DECISION (sin cambio cerrado) |
| C10 | Q7 = 2N7002K y zener de gate | APROBADO |
| C11 | C_OUT 440 uF / 12 mohm y lazo | APROBADO (sin cambio) |
| C12 | Documentacion KiCad Q1-Q4 | APROBADO (correccion documental) |

---

## C1 - Q6 (interruptor de salida): red de gate

- **Ref.:** Q6 (SiRS4401DP-T1-GE3, DigiKey/Mouser), R serie de gate, 100 k G-S, zener 12 V G-S (MM3Z12VT1G, LCSC C236177), Q7 (2N7002K, C81445) y PGOOD.
- **Actual:** R_G = 10 k, sin C gate-drain (S3 §8.1).
- **Motivo (caso S3a: cierre de SW1 contra C_carga, 9 combinaciones 7.2/12/25 V x 100/470/1000 uF):** el circuito original FALLA: caida de VOUT_INT 5.6-20.7 % (BWK=1: 9.0-20.5 %), PGOOD cae en 9/9, reconmutacion en 4/9 (12 y 25 V con 470/1000 uF). Ninguna opcion sola alcanza (C_gd 10-100 nF mejor caso 8/9; R_G 22 k-100 k mejor caso 4/9 con 100 k; RC en gate de Q7 solo enmascara el rearme y empeora la caida hasta 65 %: descartado).

Dos variantes; **Luis decide**:

### C1-A - C_gd directo 100 nF + R_G 22 k (recomendacion del primer informe)
| Pieza | Actual | Propuesta |
|---|---|---|
| C_gd (drain-gate de Q6) | no existe | 100 nF, X7R >= 50 V, SMD (numero: pendiente de verificar) |
| R_G (serie de gate) | 10 k | 22 k (no subir de ~27 k: V_GS a 6.8 V = -5.5 V) |

- **Mide (S3a final, BWK=1):** 9/9 PASA; caida max 2.19 % (4.39 % con fuente 3x lenta); PGOOD no cae; 0 reconmutaciones; I pico Q6 7.97-15.4 A.
- **V_GS a 6.8 V:** -5.56 V (25 C) / -5.47 V (100 C), margen 0.47 V sobre el minimo de 5 V. S3c: P(Q6) <= 0.172 W (limite 0.5 W).
- **Costo:** encendido pasa de 0.13-1.96 ms a 2.8-5.05 ms; **apagado con 8 A: 65 ms a 12 V y > 100 ms a 25 V**; a 25 V/8 A la SOA del apagado FALLA (u = 1.88; energia > 2.9 J). Como PGOOD tambien abre Q6, ese apagado ocurre en operacion normal.
- **Sin problema de recierre** (no hay estado que reponer).

### C1-B - C_gd con diodo en serie + Rrst a source + R_G 22 k (S3 §10)
| Pieza | Actual | Propuesta |
|---|---|---|
| C_gd | no existe | 100 nF, X7R >= 50 V, SMD (pendiente de verificar) |
| D (en serie con C_gd; anodo en nodo `m`, catodo en gate) | no existe | diodo de senal, modelado como 1N4148 de `standard.dio` (no es pieza real): numero **pendiente de verificar** |
| Rrst (nodo `m` a source = VOUT_INT) | no existe | **330 k** (unica barrida compatible con S3c); numero de pieza no aplica, SMD |
| R_G | 10 k | 22 k |

- **Mide:** apagado 7.94-8.19 ms con Rrst >= 47 k (contra 65 ms / > 100 ms de C1-A); SOA del apagado a 25 V: u = 0.205-0.22 con 0.5 uH (PASA), 0.716 con 1 mH sin diodo (AL LIMITE, ver C2). Rejilla S3a: 27/27 con 47 k, 100 k y 330 k; caida max 4.36 % (BWK=3, 330 k). Encendido a 25 V igual que C1-A (SOA no mejora, ver C3).
- **S3c (V_GS a 6.8 V, 100 C):** Rrst 10 k -2.31 V, 47 k -4.03 V, 100 k -4.67 V (FALLAN, minimo 5 V); 220 k -5.07 V (margen 0.07 V); **330 k -5.19 V (margen 0.19 V)**; 470 k -5.27 V. Margen solo nominal: no incluye tolerancias de R ni del zener.
- **Problema abierto (recierre):** el diodo deja el nodo `m` sin reponer si Q6 recierra pronto. Con Rrst 330 k: a 1 ms 6/6 pasan; a 5 ms 6/6; a 20 ms **0/6** (control sin Rrst tambien 0/6); recierre con 330 k solo seguro con GAP >= 200 ms (a 100 ms 3/4, falla 25 V/470 uF; a 50 ms 0/4). Ningun Rrst de 10 k/47 k/100 k cumple a la vez S3c y recierre a 20 ms. Exige un retardo de recierre >= 200 ms que el circuito no tiene; el hiccup del LM5175 no es simulable.
- **No simulado:** seguidor de apagado NPN (colector a source, emisor a gate); opcional.
- Descartados (S3 §10.6): Rrst 10 k con R_G 2.2 k (rejilla 7/27); variante B con reposicion a la gate (vuelve al apagado de 55.7-64.4 ms a 12 V, SOA FALLA a 25 V).

**Estado: DECISION.** Luis elige C1-A (apagado lento, SOA de apagado a 25 V/8 A FALLA u = 1.88, sin riesgo de recierre) o C1-B (apagado de ~8 ms, requiere retardo de recierre >= 200 ms y margen V_GS de 0.19 V). Cerrar con S4c.

## C2 - Diodo de rueda libre en VOUT_EXT

- **Ref.:** nuevo D en VOUT_EXT a GND (footprint; montado o DNP).
- **Actual:** no existe. (El 1N5819 de la BOM es de 1 A y no sirve para 8 A.)
- **Propuesto:** Schottky >= 40 V, clase de corriente segun carga real (medido 3.5 A pico en el peor caso a 12 V, 1.41 A a 25 V/1 mH). Modelo usado `MBR745` de `standard.dio` (no es pieza real); numero **pendiente de verificar**. SMD.
- **Motivo (caso S3b: apertura de Q6 con 8 A y carga inductiva):** sin diodo a 25 V/1 mH V_DS max = 30.7 V (AL LIMITE: > 25.6 V, margen 1.3 V; limite de diseno 32 V) y VOUT_EXT cae a -5.7 V (-9.78 V a 12 V/1 mH). Con Schottky: V_DS 25.5 V, VOUT_EXT >= -0.53 V.
- **Depende de C1:** con C1-A el apagado es lento y no hace falta (V_DS <= 25 V, VOUT_EXT >= -0.14 V). Con C1-B (apagado ~8 ms): utilizacion SOA 0.716 sin diodo (> 0.7) contra 0.66 con diodo; para 0.5 uH (y cable de 0.5/1 uH) u = 0.22 y no hace falta.
- **Estado: DECISION** (recomendado si se elige C1-B y hay cargas >= ~1 mH a 25 V; montado o DNP).

## C3 - Limite de C_carga externa a 25 V

- **Ref.:** requisito de uso del VOUT_EXT (sin pieza propia), o cambio de FET / precarga.
- **Actual:** sin limite declarado.
- **Propuesto:** C_carga externa a 25 V: **~100-150 uF para PASA (u <= 0.7), ~200 uF cota absoluta (u = 1), >= 270 uF FALLA**. A <= 12 V se admiten 1000 uF (u <= 0.31).
- **Motivo (encendido de Q6 a 25 V, SOA del datasheet digitalizada +-10 %):** u = 0.576 (100 uF), 0.717 (150), 0.96 (220), 1.15 (270), 1.38 (330), 1.79 (470); 1000 uF u = 3.1. El tiempo con V_DS > 15 V es 1.24-1.25 ms para todo C (lo fija la red de gate). Ninguna red de gate lo arregla (C1-A y C1-B dan lo mismo). El limite tiene incertidumbre **supuesta** de +-30-40 %: no se sostienen dos cifras.
- **R de precarga en paralelo con Q6: NO SIRVE como esta.** Con la carga siempre conectada u >= 1.21 (aun con 100 ohm, 5.88 W). Con carga diferida: 330 uF necesita Rpre <= 470 ohm con 10 ms (1.33 W) o <= 4.7 k con 100 ms; 1000 uF solo 100 ohm con 100 ms (6.25 W) o <= 470 ohm con 300 ms. Pierde el aislamiento de la salida y exige un retardo de cierre que no existe.
- **Convertidor real:** con ILIM ~10 A, 25 V/1000 uF ya falla electricamente (caida 9 %, PGOOD cae 5 veces, reconm. 5/5 con C1-B); 12 V/1000 uF pasa (2.08-2.22 %); 25 V/470 uF pasa criterios electricos con 3.56-3.70 % pero su SOA falla (u = 1.79-1.86). Esos casos con LM5175 real quedan para S4c.
- **Estado: DECISION** (limitar C_carga a 25 V a ~100-150 uF, o cambiar de FET).

## C4 - LED1/LED2/LED3: resistencias

- **Ref.:** R serie de LED1, LED2, LED3.
- **Actual:** 4.7 k (huella no confirmada: pendiente de verificar; el resumen supone 0603).
- **Propuesto:** misma 4.7 k en **0805** (>= 0.125 W), o subir a **>= 6.8 k** (la nueva R reduce I y por tanto brillo: verificar contra el LED real).
- **Motivo (S3c):** a 25 V, la 4.7 k de LED2/LED3 disipa 114 mW (25 C) / 120 mW (100 C); limite de un 0603 = 100 mW. A 12 V: 22.3-24.8 mW.
- **Estado: APROBADO** por simulacion; la eleccion 0805 vs >= 6.8 k es de Luis (el modelo de LED es generico de `standard.dio`, no el LED real).

## C5 - Bleeder de VOUT

- **Ref.:** bleeder 1.5 k.
- **Actual/propuesto:** 1.5 k en 2512 de 1 W: **sin cambio**.
- **Motivo (S3d):** 0.417 W a 25 V (0.552 W con FB y LED2), 0.096 W a 12 V; 25 -> 5 V en 1.07 s; 12 -> 5 V en 0.575 s.
- **Estado: APROBADO.**

## C6 - Q5 (proteccion de polaridad en la entrada)

- **Ref.:** Q5 = Vishay SiRS4401DP-T1-GE3 (P-ch, -40 V; compra DigiKey/Mouser). Pieza Rev. 1 y huella: pendiente de verificar.
- **Cambio:** ninguno de valor; el 47 k gate-GND y el zener 12 V G-S (MM3Z12VT1G, C236177) se mantienen.
- **Motivo (S1):** S1a |V_GS| max 11.84 V a 25.2 V (limite 15 V; margen 21 %); P(Q5) a 12 A max 0.387 W (6 V, 100 C; limite 1 W; punto critico V_GS = -5.75 V, la pieza esta bien encendida); S1b con BAT = -25.2 V: |V(VBUS)| 0.058 mV (25 C) / 0.532 mV (100 C) contra criterio < 0.5 V (PASA, margen 99.9 %) y fuga 2.9 nA / 0.426 uA; V_DS 25.20 V; S1c regeneracion a 5 A: 0.123-0.124 V respecto de la FEM.
- **Estado: APROBADO.** Margen de V_GS (21 %) apenas sobre el umbral del 20 %; el zener es generico y su tolerancia no esta modelada (con +-5 %, ~12.4-12.6 V, aritmetica, estimacion del resumen).

## C7 - F1 (fusible de entrada)

- **Ref.:** F1 = Littelfuse 0456025.ER (LCSC C315877): 1.92 mohm en frio, I2t de fusion 45 A2s.
- **Cambio:** ninguno.
- **Motivo (S2, caso hot-plug, limite I2t = 25 % de 45 A2s = 11.25 A2s):** I2t de F1 max 0.81 A2s sin remedio (25.2 V, 10 mohm, 0.2 uH) y 2.49 A2s con el snubber de C8 (22.1 % del limite).
- **Estado: APROBADO.** La fusion no se simula.

## C8 - Proteccion de entrada: snubber RC en VBUS (hot-plug a 25.2 V)

- **Ref.:** nuevo snubber C_sn + R_sn en serie, de VBUS a GND (cerca de C_IN). C_IN existente = 100 uF polimero (KEMET A759KS107M1V E031, 31 mohm max, `Datasheets\KEMET_A4072_A759_condensadores.pdf`) + 10 uF + 100 nF.
- **Actual:** sin snubber.
- **Propuesto (candidato, peor esquina 25.2 V / 10 mohm / 1 uH):** **C_sn 470 uF + R_sn 0.10 ohm**. C_sn polimero/electrolitico: puede ser THT (regla de Luis); numero de pieza **pendiente de verificar** (no hay datos de fabricante en `modelos/`; ESL y limite de ripple/pico no modelados).
- **Motivo (S2a, 18 casos R_bat 10/20/30 mohm x L_cable 0.2/0.5/1 uH x BAT 12.6/25.2 V):** sin snubber a 25.2 V: 5 casos FALLA y 4 AL LIMITE (0 PASA) para V(VBUS) pico < 32 V; peor 37.59 V (10 mohm, 1 uH; 37.05 V con precarga de 1 V). A 12.6 V todo PASA (peor 18.22 V). La ESR del polimero menor que el maximo empeora el pico (S2e: 40.41 V con 15 mohm); con 60 mohm ningun caso pasa con holgura. Mas polimero en paralelo (S2c, N x 100 uF) NO resuelve (35.99-37.00 V) y sube la corriente: lo que amortigua es resistencia.
- **Efecto del snubber (S2d, 18 casos a 25.2 V):** pico V(VBUS) max 28.29 V (15 mohm ESR; 27.88 V con 31 mohm), cumple < 32 V en los 18 (peor AL LIMITE, margen 11.6 %; 7 AL LIMITE, 11 PASA); a 12.6 V max 13.62 V; |V_GS| 11.84 V.
- **Pendiente de SOA de Q5:** el pico de corriente por F1/Q5 sube de **315.4 A** (sin snubber, misma precarga y ESR 31 mohm; 328.9 A desde 0 V) a **hasta 422.4 A** (25.2 V, 10 mohm, 0.2 uH, ESR 15 mohm; 387.8 A con ESR 31 mohm y 365.2 A con 20 mohm/0.2 uH/15 mohm) contra **I_DM = 350 A a 100 us** (datasheet): FALLA en 3 de 18 casos a 25.2 V (a 12.6 V el max es 195.3 A). Duracion de microsegundos. La curva SOA de Q5 a 10-100 us no se leyo (texto del PDF ilegible); la comparacion es solo contra I_DM.
- **Alternativas a evaluar:** R_sn 0.22 ohm (30.14 V en la peor esquina, pico de 111 A en el snubber y menor corriente por Q5, menos margen de tension); limitar la corriente de cierre en otro lado. Baja de 32 V solo con C_sn >= 220 uF y R_sn 0.10-0.22 ohm, o >= 470 uF con 0.05-0.22 ohm; 47-100 uF no basta.
- **Salvedades:** C_IN arranca en 1.0 V (hasta ~0.5 V optimista); R_bat y L_cable son valores de barrido, no medidos; sin rebote de contacto XT60; sin derating por DC-bias de los ceramicos.
- **Estado: SOA** (pendiente de verificar contra la SOA de Q5; el remedio no se declara resuelto) **+ DECISION** (valor de R_sn/C_sn y si se acepta el compromiso tension/corriente).

## C9 - Polaridad invertida con C_IN cargado (problema de sistema)

- **Ref.:** Q5 / C_IN / VBUS (sin cambio propuesto cerrado).
- **Hallazgo (S1d):** con el gate de Q5 a GND por 47 k, Q5 sigue encendido mientras BAT cae (constante gate-source ~0.42 ms, estimacion del resumen): V(VBUS) sigue a la bateria hasta **-25.38 V** (caso 1: +25.2 -> -25.2 V en 1 ms) y **-13.03 V** (caso 2); criterio de S1b |V(VBUS)| < 0.5 V: FALLA (informativo). Caso 3 (+25.2 -> -25.2 V en 10 us, abrupto): V(VBUS) **-39.7 V**, I(Q5) **-431 A**, P pico 314 W, V_GS -12.05 V: FALLA (informativo; el I_DM de 350 A / 100 us no se verifico aqui). C_IN (polimero y pin VIN del LM5175) quedaria cargado en inversa. Q5 en si no excede V_GS ni V_DS (criterio literal PASA). La conexion inversa en frio (S1b) SI bloquea.
- **Remedio probado (S1d_remedio_rg, caso 1, sin carga): bajar R_gate.** V(VBUS) min: 47 k -25.4 V; 22 k -18.8 V; 10 k -10.2 V; 4.7 k -4.84 V; 2.2 k -1.42 V; 1 k +0.59 V. **Ningun valor practico cumple 0.5 V**; 1 k deja 12.9 mA en el zener a 25.2 V (154 mW, estimacion = 12.9 mA x 12 V; 71 mW con 2.2 k) y V_DS inverso = 25.81 V (AL LIMITE, > 25.6 V). No se adopta.
- **Cambio propuesto:** **ninguno cerrado.** Si el escenario se considera real, la solucion es a nivel de sistema (descarga rapida de VBUS o diodo en serie), no un ajuste de R_gate; no se simulo.
- **Estado: DECISION** (Luis decide si el escenario "conexion inversa con VBUS aun cargado" es real y que solucion de sistema aplicar).

## C10 - Q7 y zener de gate

- **Ref.:** Q7 = Vishay 2N7002K-T1-GE3 (LCSC C81445); DZ1/DZ2 = onsemi MM3Z12VT1G (LCSC C236177); D1-D3 = B5819W (numero de catalogo: pendiente de verificar).
- **Cambio:** ninguno de valor. Modelos de zener/LED/Schottky son genericos de `standard.dio` (limitacion declarada); zener con Cjo = 1p en S1/S2 (la libreria trae 150p).
- **Estado: APROBADO** (S3c: V_GS Q7 = 7.35 V en todos los casos).

## C11 - C_OUT y lazo de control

- **Ref.:** C_OUT 440 uF / 12 mohm + 2 x 10 uF / 3 mohm + 100 nF / 10 mohm.
- **Cambio:** ninguno.
- **Motivo (S5, 12 -> 12 V, escalon 1 <-> 4 A, flancos 1 us):** 1 -> 4 A: undershoot 2.33 % (279 mV), asentamiento +-2 % en 89 us, 0 oscilaciones; 4 -> 1 A: overshoot 2.26 % (271 mV), 108 us, 0 oscilaciones. Criterio propuesto (< 5 % y < 1 ms) pendiente de aprobacion; ambos PASAN. Un solo punto de operacion; C_IN 30 mohm sin respaldo de datasheet en esa corrida.
- **Estado: APROBADO.**

## C12 - Documentacion KiCad (correccion documental)

- El simbolo de Q1-Q4 dice PSMN3R2-40YLD / C553274, pero la pieza montada es BUK9Y3R5-40E (marcado 93E540 confirmado; LCSC C478247 segun `HANDOFF_Proximo_Chat.md` §4). Corregir simbolo y BOM.
- **Estado: APROBADO.**

---

## Lo no simulado / abierto (afecta el cierre)
- S4b-e (convertidor completo con Q5/Q6, bleeder y PGOOD; S4c cierra Q6 contra 8 A + 470 uF con y sin el remedio de C1; S4e otros V_IN/V_OUT). Pendientes de OK de Luis sobre S1-S3.
- Seguidor NPN de apagado (C1), convertidor LM5175 real con hiccup, PGOOD con deglitch, termica acoplada, tolerancias de R y de zener.
- Convertidor idealizado en S3 (ILIM 15 A y Rd || Ls); con ILIM 10 A solo 25 V/1000 uF deja de pasar. BWK = 3 con ILIM 10 A no se corrio.
