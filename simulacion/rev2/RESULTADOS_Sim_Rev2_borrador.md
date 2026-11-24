# Resultados de simulación Rev. 2 — PCB de Potencia LM5175 (BORRADOR)

Borrador del worker de redacción; el final lo arma el orquestador. Los números salen de los `resumen.md` de S1, S2, S3 (incl. §8 y §10) y S5, y de `README_rev2.md`; ninguno se recalculó. Lo que no es un `.meas` va rotulado "(estimación)".

## 1. Alcance y reglas

- LTspice 26.0.2, `.options solver=alt method=modtrap`. Bancos: S1 polaridad (Q5), S2 hot-plug/F1, S3 salida (Q6, PGOOD/Q7, LEDs, bleeder), S4 convertidor completo, S5 escalón de carga.
- **Zener del proyecto:** `BZX84C12L` de `standard.dio` con **`Cjo=1p`**. Es una desviación de la librería, que trae `Cjo=150p`: con 150p, S2 (25.2 V) no converge. No es el MM3Z12VT1G real (sin modelo de fabricante); su tolerancia no está modelada. Ver "Inconsistencias pendientes" (S3 usa 150p).
- **Regla de margen:** AL LIMITE si el margen es < 20 % del límite; FALLA si no cumple; PASA en otro caso.
- **Límite de V_DS: 32 V** (los 40 V son el máximo absoluto de la pieza). AL LIMITE si V_DS > 25.6 V (0.8 x 32 V).
- S3 aplica además a la SOA de Q6 su propio umbral: utilización u <= 0.7 PASA, 0.7-1.0 AL LIMITE, > 1 FALLA (curvas del datasheet digitalizadas, ±10 %, TC = 25 C, sin Zth).
- **Limitación:** el convertidor de S3 es **idealizado** (fuente + limitador de 15 A + Ls ‖ Rd). No reproduce soft-start, límite por ciclo ni hiccup del LM5175. Es optimista: ILIM real ~10 A (S3 §10.7).

## 2. Tabla pasa/falla por caso

### S1 — Protección de polaridad (Q5 = SiRS4401DP)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| S1a | \|V_GS\| <= 15 V | 11.84 V (25.2 V, 25 C); 5.75 V a 6 V | PASA (margen 21 %) |
| S1a | P(Q5) <= 1 W a 12 A, 25 y 100 C | máx 0.387 W (6 V, 100 C) | PASA (margen 61 %) |
| S1a | idem con R_DS(on) de datasheet x1.5 (estimación) | 0.58 W (6 V, 100 C) | PASA (estimación) |
| S1b | \|V(VBUS)\| < 0.5 V con BAT = -25.2 V | 0.058 mV (25 C); 0.532 mV (100 C) | PASA |
| S1b | fuga < 1 mA | 2.9 nA (25 C); 0.426 uA (100 C) | PASA |
| S1b | V_DS < 32 V | 25.20 V | PASA (margen 21 %) |
| S1c | Q5 conduce a 5 A; V(VBUS)-V(BAT) < 0.2 V en régimen | V_GS -6.12 .. -11.84 V; delta 0.023-0.024 V (0.123-0.124 V respecto de la FEM) | PASA |
| S1c | pico con flanco 5 A en 5 us | 0.240 V | FALLA (informativo: L_cable x di/dt); con flanco de 100 us 0.047-0.049 V PASA |
| S1d caso 1 y 2 | \|V_GS\| <= 20 V, V_DS < 32 V | V_GS -11.84..+0.35 V; V_DS inverso 0.03 V (caso 1) / 12.2 V (caso 2, margen 62 %) | PASA |
| S1d | V(VBUS) mín contra criterio de 0.5 V de S1b | caso 1: -25.38 V; caso 2: -13.03 V | FALLA (informativo): con C_IN cargado el protector no bloquea el barrido rápido |
| S1d remedio R_gate | V(VBUS) mín | 47 k -25.4 V; 22 k -18.8; 10 k -10.2; 4.7 k -4.84; 2.2 k -1.42; 1 k +0.59 V (V_DS inverso 25.81 V, AL LIMITE) | Ningún R_gate práctico resuelve; decisión abierta |
| S1d caso 3 (extra) | +25.2 -> -25.2 V en 10 us | V(VBUS) -39.7 V; I(Q5) -431 A; P pico 314 W | FALLA (informativo; I_DM 350 A/100 us del datasheet, no verificado aquí) |

### S2 — Hot-plug y fusible F1 (sin carga, C_IN 100 uF polímero 31 mΩ + 10 uF + 100 nF)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| Barrido 25.2 V (9 casos, R_bat 10/20/30 mΩ x L 0.2/0.5/1 uH), desde 0 V | V(VBUS) pico < 32 V | 25.98 .. 37.59 V: 5 FALLA (34.67, 37.59, 32.21, 35.43, 33.56 V), 4 AL LIMITE (30.52, 27.86, 25.98, 30.18 V), 0 PASA | FALLA |
| Barrido 12.6 V (8 casos desde 0 V) | idem | 13.50 .. 18.22 V (peor, margen 43 %) | PASA |
| Caso 12.6 V / 30 mΩ / 0.2 uH (el 18.º) | idem | 12.60 V, solo con precarga 1.0 V (con C_IN en 0 V no converge) | PASA, informativo |
| I²t en F1 | < 11.25 A²s (25 % de 45 A²s) | máx 0.81 A²s (25.2 V) | PASA (7.2 % del límite) |
| \|V_GS\| Q5 | <= 15 V | 11.84 V (25.2 V); 11.51 V (12.6 V) | PASA (margen 21.1 %) |
| V_DS inverso Q5 | < 32 V | máx 25.68 V y 25.62 V (25.2 V, 10 mΩ, 1/0.5 uH); resto <= 24.90 V; 12.6 V máx 19.09 V | AL LIMITE en 2 casos; resto PASA |
| Pico de corriente vs I_DM = 350 A (100 us) | < 350 A | 328.9 A (25.2 V, 10 mΩ, 0.2 uH); 12.6 V máx 159.3 A | AL LIMITE (6.0 % bajo 350 A) |
| Sensibilidad ESR 15/31/60 mΩ (25.2 V, 4 esquinas) | V(VBUS) pico < 32 V | ESR 15: 34.83 .. 40.41 V; ESR 60: 29.20 .. 34.02 V (2 AL LIMITE, 2 FALLA) | FALLA con ESR 15 y 31 mΩ; ESR 60 no pasa con holgura |
| Remedio A: snubber RC (20 celdas, esquina 25.2 V/10 mΩ/1 uH, referencia sin remedio 37.05 V) | V < 32 V | baja de 32 V solo con C_sn >= 220 uF y R_sn 0.10-0.22 Ω, o C_sn >= 470 uF; 47-100 uF no basta | parcial |
| Remedio B: N x 100 uF de polímero extra | idem | N=1/2/4: 37.00/36.71/35.99 V; con N=4 pico 389.3 A > I_DM | FALLA (no sirve) |
| Remedio C: snubber 470 uF + 0.10 Ω (18 casos 25.2 V, 18 casos 12.6 V; precarga 1.0 V) | V(VBUS) pico < 32 V | 25.2 V: máx 28.29 V (ESR 15 mΩ), 27.88 V (ESR 31); 12.6 V: máx 13.62 V; I²t máx 2.49 A²s | Tensión: AL LIMITE (peor margen 11.6 %) |
| Remedio C, pico de corriente vs I_DM = 350 A | < 350 A | máx 422.4 A (también 387.8 A y 365.2 A); 12.6 V máx 195.3 A | FALLA en 3 de 18 casos a 25.2 V; **pendiente de SOA de Q5 a 10-100 us** |

Los números de remedio (precarga 1.0 V) son hasta ~0.5 V optimistas frente a 0 V.

### S3 — Salida (Q6, PGOOD/Q7, bleeder). Convertidor idealizado; zener con Cjo=150p

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| S3a base (R_G 10 k, sin remedio), rejilla 9 | caída VOUT_INT <= 5 %; PGOOD no cae; sin reconmutación | caída 9.0-20.5 %; PGOOD cae 9/9; reconmuta 4/9 (12 y 25 V con 470/1000 uF) | FALLA |
| S3a final (C_gd 100 nF directo + R_G 22 k) | idem | 9/9 pasan; caída máx 2.19 % (BWK=1), 4.39 % (fuente 3x más lenta) | PASA |
| S3a final, SOA del encendido | u <= 0.7 | 25 V: 100 uF 0.576; 150 uF 0.72; 220 uF 0.96; 330 uF 1.37; 470 uF 1.79; 1000 uF 3.14; a <= 12 V u <= 0.31 | FALLA a 25 V con C >= 330 uF; AL LIMITE 150-220 uF |
| S3a, red nueva (diodo 1N4148 + Rrst), rejilla de 27 | idem | 27/27 con Rrst 47 k/100 k/330 k; caída máx 4.36 % (330 k, BWK=3); con 10 k solo 18/27 válidos (Q6 no cierra a 7.2 V) | PASA (47 k/100 k/330 k) |
| S3a recierre (Rrst 330 k) | pasan las 3 | GAP 1 ms 6/6; 5 ms 6/6; 20 ms 0/6; 12/25 V x 470/1000 uF: 50 ms 0/4, 100 ms 3/4 (falla 25 V/470 uF), >= 200 ms 4/4 (V(m)-V(vint) <= 0.11 V) | PASA solo con GAP >= 200 ms |
| S3a con ILIM = 10 A (con y sin Rd) | idem | solo falla 25 V/1000 uF (caída 9 %); 25 V/470 uF 3.56 %; 12 V/1000 uF 2.08 % | FALLA 25 V/1000 uF |
| S3b sin remedio, sin diodo | V_DS < 32 V | máx 30.73 V (25 V, 1 mH); VOUT_EXT mín -9.78 V (12 V, 1 mH); u 0.716 | AL LIMITE (25 V/1 mH); resto PASA |
| S3b con Schottky MBR745 | V_DS < 32 V | 25.5 V; VOUT_EXT mín -0.53 V; u 0.66; I pico diodo 1.41 A (25 V), 3.5 A (12 V) | PASA (justo bajo 25.6 V) |
| S3b con C_gd directo 100 nF, 25 V/8 A | SOA del apagado | t(I<10 %) 65 ms (12 V), > 100 ms (25 V); u = 1.88 | FALLA |
| S3b red nueva (Rrst >= 47 k) | apagado <= ~8 ms; u < 0.7 | 7.94-8.19 ms; u 0.22 (0.5 uH), 0.716 (1 mH sin diodo), 0.66 con diodo | PASA con 0.5 uH; AL LIMITE con 1 mH sin diodo |
| S3c sin remedio | 5 <= \|V_GS Q6\| <= 15 V; P(Q6) <= 0.5 W | 6.12 V (6.8 V) .. 12.0 V (25 V); P 0.107-0.166 W | PASA |
| S3c con C_gd 100 nF + R_G 22 k | idem | \|V_GS\| 5.47 V (6.8 V, 100 C) .. 11.9 V; P <= 0.172 W | PASA (margen 0.47 V a 6.8 V) |
| S3c red nueva, Rrst a source (100 C, 6.8 V) | \|V_GS\| >= 5 V | 10 k -2.31 V; 47 k -4.03; 100 k -4.67; 220 k -5.07; 330 k -5.19 V | FALLA 10 k/47 k/100 k; PASA >= 220 k (margen 0.07 V con 220 k, 0.19 V con 330 k, nominal) |
| S3d bleeder 1.5 k | 25 -> 5 V en ~1.1 s; P ~ 0.42 W | 1.07 s; 0.417 W (0.552 W con FB y LED2); 12 V: 5 V en 0.575 s | PASA |
| S3e descarga C_IN (sin convertidor) | informativo | desde 12 V: 5 V en 0.58 s, 1 V en 14.8 s; desde 25.2 V: 5 V en 1.0 s, 1 V en 15.3 s; LED1 < 10 uA a 2.84/3.26 s | informativo |
| S3f precarga con R en paralelo con Q6 | SOA del encendido | carga siempre conectada: V_DS al cerrar 24.2-24.9 V, u >= 1.21; carga diferida: solo con >= 100 ms o <= 470 Ω | No sirve como está |
| S3 LEDs (4.7 k a 25 V) | P(R) <= 0.1 W (0603) | 114-120 mW | FALLA en 0603; usar 0805 o >= 6.8 k |

### S5 — Escalón de carga 1 <-> 4 A, 12 -> 12 V (criterio propuesto, pendiente de aprobación: desviación < 5 %, asentamiento ±2 % < 1 ms)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| 1 -> 4 A | desviación < 5 %; asentamiento < 1 ms; sin oscilaciones | undershoot 2.33 % (279 mV); asentamiento ±2 % 89 us; 0 oscilaciones | PASA (47 % y 9 % del límite) |
| 4 -> 1 A | idem | overshoot 2.26 % (271 mV); asentamiento ±2 % 108 us; 0 oscilaciones; I(L1) mín -0.24 A | PASA (45 % y 11 % del límite) |

Salvedades de S5: vpre del evento 1 no es plano (-20.1 mV); carga de corriente constante con flancos de 1 us; C_SS acelerado (12 nF); un solo punto de operación (12 -> 12 V).

## 3. Lo no simulado y por qué

| Tema | Por qué |
|---|---|
| Hiccup del LM5175 | No es simulable con el modelo TI. El recierre seguro de la red de gate nueva (>= 200 ms) depende de él. |
| Térmica | Sin autocalentamiento; solo temperatura de unión impuesta (25/100 C) escalando el modelo Vishay. SOA a TC = 25 C (optimista si la placa se calienta), sin Zth. F1 constante en frío; la fusión no se simula (solo I²t). |
| Zener, LED, Schottky, 1N4148 | Modelos genéricos de `standard.dio`, no las piezas reales (MM3Z12VT1G, LED real, B5819W). Zener con Cjo=1p. |
| Convertidor idealizado en S3 | Rd ‖ Ls da corriente instantánea; ILIM 15 A frente a ~10 A reales. BWK = 3 no se corrió con ILIM = 10 A (margen con fuente lenta sin medir). |
| C_IN sin datos de datasheet | S4a/S5 usan 100 uF / 30 mΩ "no respaldado"; S1/S2 usan el KEMET A759 (31 mΩ). Ver Inconsistencias. |
| Polaridad invertida con C_IN cargado | V(VBUS) -39.7 V, -431 A (S1d caso 3); sin remedio válido (R_gate no lo resuelve). Es escenario de sistema. |
| SOA de Q5 con 422 A de pico | S2 comparó solo contra I_DM = 350 A (100 us); la curva SOA a 10-100 us no se evaluó para Q5. El snubber 470 uF + 0.10 Ω no puede darse por resuelto. |
| Arranque con rampa de BAT (S1) y conexión lenta | No simulados (en S2 el único caso con rampa no converge). |
| Rebote del XT60, ESL, derating de cerámicos por DC bias | Sin modelar. |
| Seguidor NPN de apagado en la gate de Q6 | Opcional, no simulado. |
| Tolerancias de R y zener en V_GS a 6.8 V | Margen de 0.19 V (Rrst 330 k) es nominal. |
| Diodo de rueda libre con la red nueva a L = 100 uH | No se corrió; solo el dato sin remedio (§4 de S3). |

## 4. Piezas

| Ref | Pieza | Fabricante | Número de compra |
|---|---|---|---|
| Q1-Q4 | BUK9Y3R5-40E (marcado 93E540 confirmado en placa) | Nexperia | LCSC: pendiente de verificar (no figura en README ni en los resúmenes) |
| Q5, Q6 | SiRS4401DP-T1-GE3 (P-ch -40 V, 2.2 mΩ máx @ -10 V) | Vishay | DigiKey/Mouser; número de catálogo pendiente de verificar |
| Q7 | 2N7002K-T1-GE3 | Vishay | LCSC C81445 |
| F1 | 0456025.ER | Littelfuse | LCSC C315877 |
| DZ1, DZ2 | MM3Z12VT1G (12 V, SOD-323) | onsemi | LCSC C236177 |
| D1-D3 | B5819W | pendiente de verificar | LCSC pendiente de verificar (ver Inconsistencias: 1N5819 vs B5819W) |
| C_IN (polímero) | A759KS107M1V E031 (100 uF/35 V, 8x12 mm) | KEMET | LCSC/DigiKey pendiente de verificar |
| L1 | 4.7 uH / 4.8 mΩ | pendiente de verificar | pendiente de verificar |
| D de rueda libre (opcional, DNP) | Schottky >= 40 V; simulado con MBR745 genérico | pendiente de verificar | pendiente de verificar |
| D en serie con C_gd (red nueva) | simulado con 1N4148 genérico | pendiente de verificar | pendiente de verificar |
| LED1-3 | resistencias 0805 o >= 6.8 k | — | pendiente de verificar |

## 5. S4 — convertidor completo

PENDIENTE: S4b–e en curso. Casos previstos (sin números aquí):

- S4a: línea base 12 -> 12 V, 8 A (hecha; ver `S4_convertidor\resumen_S4a.md`).
- S4b: con Q5 (+ R_bat, L_cable), bleeder y PGOOD, Q6 abierto; diferencia < 2 % contra S4a; PGOOD al final del soft-start.
- S4c: cierre de Q6 contra 8 A + 470 uF, con y sin el remedio elegido (árbitro real de S3a): caída < 9 % (ideal < 5 %), sin rearme, valle de I(L1) < 10.6 A.
- S4d: apertura de Q6 con 8 A: sobreimpulso < 7.5 % (ideal < 5 %).
- S4e: S4c/S4d en 7.4 -> 10 V y 16.8 -> 7.2 V (y 22.2 -> 24 V si alcanza).

## 6. Decisiones abiertas para Luis

| # | Decisión | Opciones | Recomendación del orquestador |
|---|---|---|---|
| 1 | Red de gate de Q6 | (a) C_gd 100 nF directo + R_G 22 k: sin problema de recierre, pero apaga en 65 ms / > 100 ms y la SOA del apagado a 25 V/8 A da u = 1.88 (FALLA). (b) Diodo 1N4148 en serie con C_gd + Rrst a source: apaga en ~8 ms, pero el encendido lento solo se recupera con GAP >= 200 ms entre apertura y recierre. | (b) con **Rrst 330 kΩ** y **GAP >= 200 ms**. Rrst < 220 k incumple S3c; margen de V_GS 0.19 V, nominal. |
| 2 | Límite de C_carga a 25 V | Hasta 100 uF (u 0.576, PASA); 150 uF (u 0.717, AL LIMITE); ~220 uF cota absoluta (u 0.96); >= 270 uF FALLA. Más exige otro FET o precarga de cientos de ms. Incertidumbre de ±30-40 % supuesta, no medida. | **150 uF a 25 V** (según el orquestador). Ojo: con la regla de S3 ese valor es AL LIMITE, y S3 §8.2 sugería <= 100 uF para no estar al límite. |
| 3 | Diodo de rueda libre en VOUT_EXT | Montado (Schottky >= 40 V) o DNP. Sin diodo, 25 V/1 mH: V_DS 30.7 V y u 0.716, AL LIMITE. | **Montado.** |
| 4 | Resistencias de LED1-3 | 0805 con 4.7 k, o >= 6.8 k en 0603. | **0805 con 6.8 k.** |
| 5 | Snubber de S2 (470 uF + 0.10 Ω) | Corrige la tensión (<= 28.29 V) pero sube el pico de corriente a 422.4 A (> I_DM 350 A). Alternativas: R_sn 0.22 Ω (30.14 V en la peor esquina, menos corriente), limitar corriente de cierre en otro lado. | **Pendiente de la SOA de Q5 a 10-100 us**; no adoptarlo todavía. |
| 6 | Protección contra barrido inverso rápido con C_IN cargado (S1d) | Ajustar R_gate no basta (1 k deja +0.59 V y V_DS 25.81 V). Requiere descarga rápida de VBUS o diodo en serie a nivel de sistema, o aceptar el escenario como no exigible. | Sin recomendación del orquestador; decidir si el escenario es real. |

## 7. Inconsistencias pendientes

No se resolvieron; se anotan para el orquestador.

1. **Cjo del zener:** S1 y S2 usan `Cjo=1p`; S3 §1 dice usar "copia literal de `standard.dio`" con `Cjo=150p` en el banco de salida; S5 advierte que `Cjo=1p` no coincide con la librería. README tampoco fija el valor.
2. **ESR/dato de C_IN:** S1/S2 identifican el KEMET A759KS107M1V E031 y usan 31 mΩ del datasheet; S5 (y S4a, según ESTADO) usan 30 mΩ y lo declaran "no respaldado, pendiente de datasheet".
3. **Diodos D1-D3:** README dice B5819W; S3 §1 habla del "1N5819 de la BOM" (1 A); S5 usa `1N5819` como "sustituto de B5819W"; ESTADO lista "Schottky 1N5819" entre las correcciones aprobadas.
4. **SOA de Q5/Q6:** S2 dice que la curva SOA del SiRS4401DP no se leyó (texto del PDF ilegible) y compara solo con I_DM; S3 digitalizó esa curva (página 4 del mismo datasheet) para Q6.
5. **Umbral de SOA:** S3 usa u <= 0.7 como PASA (30 % de margen), distinto de la regla común de 20 %.
6. **Límite de C_carga:** S3 §8.2 sugiere <= 100 uF para no estar al límite a 25 V; S3 §10.8 y §10.9 dan ~100-150 uF; la recomendación del orquestador (150 uF) cae en AL LIMITE (u 0.717).
7. **Recomendación de gate dentro de S3:** §8.1 recomienda C_gd directo; §10.9 presenta la red con diodo + Rrst como alternativa y deja la elección a Luis.
8. **Rango de caída en S3a base:** ESTADO_PAUSA dice 9-20 %; S3 reporta 5.6-20.7 % (todas las velocidades de fuente) y 9.0-20.5 % con BWK = 1.
9. **ESTADO_PAUSA desactualizado:** lista S1 sin resumen, S2 sin correr y S3f sin lanzar; los `resumen.md` de S1, S2 y S3 §10 ya existen.
10. **Criterio de S5:** el handoff solo pide "reportar"; el criterio (< 5 %, < 1 ms, umbral de 80 %) es propuesto por el banco y no está aprobado.
11. **Número LCSC de Q1-Q4:** solo aparece en `HANDOFF_Proximo_Chat.md` (C478247, y símbolo KiCad PSMN3R2-40YLD/C553274), no en README ni en los resúmenes; no se usa aquí.
