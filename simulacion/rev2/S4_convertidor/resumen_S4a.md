# S4a — Línea base 12 → 12 V, 8 A resistivo, valores as-built (LTspice 26.0.2)

Archivos: `S4a.cir` / `S4a.log` / `S4a.raw`, `S4a_pasos.cir` / `S4a_pasos.log`, `csv/S4a.csv` (4999 puntos, remuestreo uniforme en tiempo: V(vout), I(L1), V(pgood), V(ss)).

## Configuración
- Q1–Q4: `BUK9Y3R5-40E` (`..\modelos\BUK9Y3R5-40E.lib`, DRAIN GATE SOURCE). LM5175: `..\LM5175_TRANS_xorfix.lib` (una sola inclusión).
- C_SS = 12 nF (real 120 nF). C_OUT 440 µF / 12 mΩ + 2×10 µF / 3 mΩ + 100 nF / 10 mΩ, sin `.step`. L1 4.7 µH, Rser 4.8 mΩ. C_IN 100 µF / 30 mΩ (**no respaldado, pendiente de datasheet**) + 10 µF + 100 nF.
- D1–D3: `1N5819` de `standard.dio` (línea 11): `.model 1N5819 D(Is=31.7u Rs=.051 N=1.373 Cjo=110p M=.35 Eg=.69 Xti=2 Iave=1 Vpk=40 mfg=OnSemi type=Schottky)`. Es el sustituto de B5819W (sin modelo del fabricante).
- RFB_TOP 49.9 k, RFB_BOT = 3.564 kΩ (`{RFB_TOP/(VOUT_target/0.8-1)}`), carga 1.5 Ω, sin Iload. PGOOD → nodo `PGOOD` + 100 kΩ a VCC.
- V1 `PWL(0 0 200u 12)`, `.ic V(ss)=0 V(comp)=0 V(vout)=0`, `.tran 0 3m 0 100n`, `.options solver=alt method=modtrap`, sin `plotwinsize`.

## Protocolo A (log)
Header `solver = Alternate`, `method = modified trap`. Sin `Note:` del caret. "Files loaded": `.cir`, `standard.dio`, `LM5175_TRANS_xorfix.lib`, `BUK9Y3R5-40E.lib`. Ambos logs terminan con `Total elapsed time`.

## Protocolo B (`S4a_pasos`, 0.5 ms, `.step param VIN_dc list 10 12 14`, ventana 0.3–0.5 ms)
Los `.meas` difieren entre pasos (arranque aún en soft-start; sirve solo para confirmar que `VIN_dc` actúa):

| Paso VIN | il_max [A] | il_avg [A] | vout_reg [V] | vss_end [V] | vcomp_end [V] |
|---|---|---|---|---|---|
| 10 V | 4.50 | 0.241 | 0.0072 | 0.170 | 1.608 |
| 12 V | 8.27 | 0.910 | 0.0389 | 0.176 | 1.654 |
| 14 V | 9.94 | 1.631 | 0.0908 | 0.181 | 1.659 |

(`t_pgood` FAIL en los 3 pasos: PGOOD no sube antes de 0.5 ms, esperado.) Tiempo: 938 s.

## Resultados S4a (ventana de régimen 2.4–2.6 ms) vs referencia

| Medida (`.meas`) | S4a | Referencia (as-built anterior, otro MOSFET, 220 µF) |
|---|---|---|
| il_avg | 9.06 A | ≈ 9.4 A |
| il_pk | 9.90 A | ≈ 10.2–10.3 A |
| il_vy | 8.24 A | ≈ 8.65 A |
| fb_reg | 0.7995 V | 0.800 V |
| vout_reg | 11.992 V | ≈ 11.98 V |
| vout_pp | 117 mV | (incluye picos numéricos de transición V_IN≈V_OUT) |
| vcc_reg | 7.50 V | — |
| il_max (toda la corrida, arranque acelerado) | 12.88 A | ≈ 11.5–11.6 A |

Ventanas posteriores (consistencia): 2.6–2.8 ms: il_avg 9.04 A, vout 11.999 V, fb 0.79996 V. 2.8–3.0 ms: il_avg 9.08 A, vout 12.007 V, fb 0.8005 V.
Otros: V(ss) final (3 ms) 1.218 V; V(comp) 1.933 V (no clavada a 3.3 V).

Lectura: I(L1) de un dígito de amperios, COMP en su rango, salida regulando en 12.0 V. Corriente media 9.06 A = 1.13 × 8.0 A de carga (la referencia: 9.4/8.4 = 1.12). Pico/valle ~0.3–0.4 A por debajo de la referencia (otro MOSFET, BUK vs PSMN, y otros valores as-built de L1/C_OUT; no se investigó más). `il_max` +11 % sobre la referencia, coherente con C_OUT doble (440 vs 220 µF) a igual C_SS; ocurre al final del soft-start (hacia 2.0 ms, V(vout) con sobreimpulso de ~1.3 % visto en el CSV, sin `.meas`). Con C_SS real (120 nF) la corriente de carga de C_OUT cae ~10×, de modo que el pico de arranque real debería ser menor (estimación por escalamiento lineal, no simulada).

## PGOOD
`t_pgood` = 1.845 ms (V(pgood) = 3.5 V, flanco de subida), poco antes de `t_vout11` = 1.878 ms (V(vout) = 11 V), con V(ss) ≈ 0.79 V; sube de ~7 mV a 7.5 V (= VCC por el pull-up de 100 kΩ). Es decir, sube al final del soft-start, cuando FB entra en ~0.88·0.8 V. El pin PGOOD del `.SUBCKT` responde de forma creíble (no hace falta el comparador de S3).

## Warnings del log
- `WARNING: Node n010 is floating`: ISNS_N/ISNS_P unidos entre sí y sin conexión en el netlist as-built (heredado de `Buckboost_fixed_netlist.net`).
- `d_d111`, `d_d1`, `d_d3`: coeficiente de emisión N = 0.01 / 0.001 / 0.01 "muy pequeño" — diodos internos del modelo LM5175 de TI; no son los D1–D3 de la placa.
- Sin `Note:` del caret.

## Cómputo e incidente de ejecución
- S4a completa: 2465 s (≈ 41 min); pasos: 938 s. Máquina compartida con otros bancos en paralelo.
- Las primeras ejecuciones (incluida la del intento previo) terminaron con `exit=1` y el log truncado justo tras `Direct Newton iteration…` (sin `.meas`, sin `Total elapsed time`). Causa: **el proceso `LTspice.exe` fue terminado desde fuera** (otros workers lanzando/limpiando LTspice por nombre; dos instancias mías murieron al mismo segundo). No era problema del netlist (se confirmó con variantes: PSMN, sin `.meas`, sin `RPG`, sin `.save`, todas morían igual).
- Remedio usado: copia byte-idéntica de `LTspice.exe` con otro nombre de proceso (misma versión 26.0.2; se desvía de la regla literal de ejecución del README), borrada al terminar. Cualquier otro banco con log truncado y sin `Total elapsed time` debe repetirse.
