# S5 — Escalón de carga 1 → 4 A y 4 → 1 A, 12 V → 12 V (LTspice 26.0.2)

Archivos: `S5.cir` / `S5.log` / `S5.raw` (256 MB, regenerable), `postproceso.py` + `postproceso_salida.txt` (aritmética sobre los `.meas` y contraste), `csv/S5.csv` (6000 puntos, 3.4–6.5 ms: V(vout), V(vf50), V(vsl), V(fb), V(ss), V(comp), V(pgood), V(vcc), V(vin), I(L1), I(R7), I(I1)).

## Estado de la corrida
Una sola corrida contiene los dos eventos (1→4 A a 3.5 ms, 4→1 A a 5.0 ms, fin 6.5 ms). `S5.log` válido: termina en `Total elapsed time: 7249.868 seconds`, sin "tolerance relaxed" ni `Note:` del caret; cabecera `solver = Alternate`, `method = modified trap`. Warnings heredados de S4a (ISNS flotante `n010`; N pequeño en los diodos internos del modelo TI). Corrida lanzada con `Start-Process -PassThru` (PID propio); no se mató ningún proceso.

## Configuración (base `S4_convertidor\S4a.cir`)
- LM5175 `..\LM5175_TRANS_xorfix.lib` (una inclusión), Q1–Q4 `BUK9Y3R5-40E`, L1 4.7 µH / 4.8 mΩ, D1–D3 `1N5819` (sustituto de B5819W), C_SS 12 nF (acelerado; real 120 nF), ISNS igual que S4a. V1 `PWL(0 0 200u 12)`, `.ic V(ss)=0 V(comp)=0 V(vout)=0`, `.tran 0 6.5m 0 100n`, `.options solver=alt method=modtrap`.
- **C_OUT:** el banco pide ESR 12 mΩ y S4a ya usa 440 µF / Rser = 12 mΩ (C8), así que es la misma ESR; sin `.step`. Se mantienen en paralelo 2×10 µF / 3 mΩ y 100 nF / 10 mΩ (como S4a).
- **Carga:** R7 = 12 Ω fija (1 A) + fuente de corriente ideal `I1` PWL: 0 hasta 3.5 ms, +3 A hasta 5.0 ms, 0 después; **flancos de 1 µs** (declarado, no hay dato de banco). Corriente medida (`.meas`): 0.9995 A → 4.000 A → 0.9999 A. Al ser corriente constante, la carga de 4 A no baja con V(vout) (una resistencia lo haría ligeramente).
- C_IN 100 µF / 30 mΩ: sigue **no respaldado, pendiente de datasheet** (igual que S4a).
- Zener BZX84C12L: **no aplica** (no hay zener en este netlist). Aviso: `standard.dio` define `BZX84C12L` con `Cjo=150p`, no `1p`; la instrucción de `Cjo=1p` no coincide con la librería (pendiente de aclarar para los bancos que sí lo usen).

## Sondas y definiciones (todo número sale de un `.meas` o de aritmética directa sobre ellos)
- `vpre` = AVG de V(vout) en los 200 µs previos al evento (3.3–3.5 ms; 4.8–5.0 ms). Undershoot/overshoot = (vpre − MIN)/vpre y (MAX − vpre)/vpre, en %.
- **Un `.meas` no calcula medianas.** Pico y rizado se toman de `V(vf50)`: sonda con buffer ideal + **RC de 50 ns (no es una mediana)**, que atenúa los picos numéricos de ~5 ns. Se reporta también el MIN/MAX crudo de V(vout). Contraste (estimación): mediana de 50 ns por postproceso del `.raw` → 2.340 % / 2.259 %, es decir coincide con el RC.
- Asentamiento: último cruce de la banda (vpre ± x %) por `V(vsl)` (RC de 10 µs sobre V(vout); agrega ≲ 10 µs de retardo), medido desde el inicio del evento. "Sale" = sí hubo cruce. Se reportan ±2 % (el pedido) y ±1 % / ±0.5 % como suplemento.
- Oscilaciones: cruces de las bandas ±0.5 % por `V(vsl)`; cada excursión fuera de banda = un cruce de salida + uno de reentrada; oscilaciones = ciclos completos arriba y abajo = min(excursiones arriba, excursiones abajo).

## Resultados

| Medida | 1 → 4 A (3.5 ms) | 4 → 1 A (5.0 ms) |
|---|---|---|
| vpre (V) | 11.9936 | 11.9987 |
| Deriva de vpre en 200 µs previos (mV) | −20.1 (no plano, ver nota) | −0.096 (plano) |
| Rizado pp previo, V(vf50) (mV) | 26.8 | 37.9 |
| MIN V(vf50) / MAX V(vf50) (V) | 11.7146 / 12.0361 | 11.9640 / 12.2698 |
| **Undershoot** (% / mV) | **2.33 % / 279 mV** | 0.29 % / 35 mV (≈ rizado previo) |
| **Overshoot** (% / mV) | 0.35 % / 43 mV (≈ rizado previo) | **2.26 % / 271 mV** |
| Undershoot / overshoot crudos (V(vout)) | 2.37 % / 0.36 % | 0.37 % / 2.26 % |
| **Asentamiento ±2 %** | **89 µs** | **108 µs** |
| Asentamiento ±1 % / ±0.5 % | 165 µs / 198 µs | 177 µs / 222 µs |
| Vuelve a vpre en 1.5 ms (V(vend) − vpre) | +0.043 % | +0.0005 % |
| **Oscilaciones** (ciclos arriba+abajo fuera de ±0.5 %) | **0** (1 excursión, sin rebote) | **0** (1 excursión, sin rebote) |
| I(L1) pico / valle en el evento (A) | 5.54 / 0.71 | 4.55 / **−0.24** |
| I(L1) prom / mín en meseta previa (A) | 1.085 / 0.687 | 4.220 / 3.869 |

Lectura: el escalón produce una sola excursión de ≈ 2.3 % que regresa a vpre en ~0.1 ms sin sobrepasar la banda de ±0.5 % del lado opuesto. El mínimo de I(L1) en la meseta de 1 A es +0.69 A (el rizado no llega a cero); en 4 → 1 A la corriente del inductor baja a −0.24 A durante el evento (medido). No se afirma el modo (CCM/DCM/FCCM) por el MODE R12 = 93.1 kΩ: pendiente de verificar contra datasheet.

## Veredicto
Criterio de S5 **(criterio propuesto, pendiente de aprobación)**: la documentación (`PCB_Potencia_Sim_Rev2_Handoff.md` §S5) solo pide "reportar" sobreimpulso, asentamiento y oscilaciones. Se propone: desviación máxima (undershoot u overshoot) < 5 % de vpre y asentamiento (±2 %) < 1 ms. PASA si < 80 % de ambos límites (< 4 %, < 0.8 ms); AL LIMITE si entre 80 y 100 %; FALLA si se supera alguno. (Umbral de 80 % fijado antes de ver resultados.)

| Escalón | Desviación máx. (vs 5 %) | Asentamiento ±2 % (vs 1 ms) | Oscilaciones | Estado |
|---|---|---|---|---|
| 1 → 4 A | 2.33 % (47 % del límite) | 89 µs (9 %) | 0 | **PASA** |
| 4 → 1 A | 2.26 % (45 % del límite) | 108 µs (11 %) | 0 | **PASA** |

## Salvedades
1. **Evento 1 con vpre no plano:** el promedio 3.1–3.3 ms es 12.0136 V y 3.3–3.5 ms es 11.9936 V (−20 mV, −0.17 %): el arranque (C_SS acelerado) aún se asienta. Efecto sobre el 2.33 % ≲ 0.2 puntos; el evento 2 parte de una meseta plana (−0.1 mV) y no está contaminado.
2. El evento 2 parte del estado posterior al evento 1 (consecutivos, 1.5 ms de separación); la reentrada a vpre del evento 1 es +0.043 %, así que ambos son comparables.
3. Carga de corriente constante con flancos de 1 µs: un banco real con carga electrónica puede tener otro di/dt; con una resistencia o flancos más lentos el resultado sería menos severo (estimación, no simulado).
4. Arranque con C_SS 12 nF; C_IN (30 mΩ), D1–D3 (1N5819) y la ESR de C_OUT son los valores de S4a; ripple real y picos de la placa pueden diferir. Un solo punto de operación (12 → 12 V, V_IN ≈ V_OUT, zona de transición buck-boost); no se simuló otro banco ni otras ESR.
5. Los números de la sonda RC de 10 µs (asentamientos) incluyen su retardo (≲ 10 µs). El hiccup no es simulable.
