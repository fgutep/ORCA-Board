# Simulación Rev. 2 — PCB de Potencia (LM5175)

Validación en LTspice 26.0.2 de los agregados de la Rev. 2 (ver `PCB_Potencia_Sim_Rev2_Handoff.md` y `PCB_Potencia_Mejoras_Rev2.md`).

## Estructura

| Carpeta / archivo | Contenido |
|---|---|
| `LM5175_TRANS_xorfix.lib` | Modelo TI con el fix del `^` (verificado: difiere de la original solo en líneas 1040, 1185-86, 1208-09) |
| `xnor_test.cir`, `xnor_test_orig.cir` | Prueba del fix (xorfix → 1,0,0,1 / 0,1,1,0; original → 1,1,1,0) |
| `Buckboost_fixed_netlist.net` | Netlist del `.asc` vigente (base para S4/S5) |
| `modelos/` | Modelos SPICE y datasheets de fabricante (solo lectura para los bancos) |
| `S1_polaridad/` | Q5 protección de polaridad |
| `S2_hotplug/` | Conexión en caliente, fusible F1, inrush |
| `S3_salida/` | Q6 interruptor de salida, PGOOD, bleeder |
| `S4_convertidor/` | Convertidor completo con LM5175 + agregados |
| `S5_escalon/` | Escalón de carga 1↔4 A |
| `RESULTADOS_Sim_Rev2.md` | Resultado final |

La corrida de referencia de la Rev. 1 (barrido de 4 esquinas, `../resultados/c2_4esquinas_resumen.*`) está documentada en el `README.md` de la raíz; el barrido de 16 esquinas quedó como corrida preliminar descartada.

Cada carpeta de banco: `.cir` (fuente), `.log` (con `.meas`), `resumen.md` (tabla pasa/falla del banco), `csv/` (formas de onda de casos al límite o fallidos). `.raw` y `.db` son regenerables.

## Reglas comunes de los bancos

- Ejecutar: `"C:\Program Files\ADI\LTspice\LTspice.exe" -b <ruta absoluta>.cir` (bloquea; el `.log` sale en texto plano junto al `.cir`). Primera línea del `.cir` = título.
- Todo `.cir` lleva `.options solver=alt method=modtrap` (fija Alternate + modified trap sin depender del `LTspice.ini`).
- Con LM5175: solo `LM5175_TRANS_xorfix.lib` (una sola inclusión; el log NO debe mostrar el `Note:` del caret), V1 en rampa `PWL(0 0 200u {VIN})`, `.ic V(ss)=0 V(comp)=0 V(vout)=0`, nodos `SS` y `COMP`, sin `uic`. El hiccup no es simulable.
- Antes de un `.step` largo: 3 pasos cortos y confirmar que los `.meas` difieren.
- Todo número reportado sale de un `.meas` o de aritmética directa sobre ellos; lo demás va rotulado "(estimación)". Si un criterio falla, no se ajusta en silencio: se reporta y el remedio va como caso aparte.
- No inventar parámetros: modelos y datos de fabricante (en `modelos/`). Si falta un modelo, se dice.

## Piezas usadas en simulación

| Ref | Pieza | Modelo | Fuente |
|---|---|---|---|
| Q1–Q4 | Nexperia BUK9Y3R5-40E (marcado 93E540 confirmado en placa) | `modelos/BUK9Y3R5-40E.lib`, `.SUBCKT BUK9Y3R5-40E DRAIN GATE SOURCE` | Nexperia (Transim) |
| Q5, Q6 | Vishay SiRS4401DP-T1-GE3 (P-ch −40 V, 2.2 mΩ máx @ −10 V, 2.9 mΩ máx @ −4.5 V); compra DigiKey/Mouser | `modelos/SiRS4401DP_PS.TXT`, `.subckt SiRS4401DP d g s` | Vishay |
| Q7 | Vishay 2N7002K-T1-GE3 (LCSC C81445) | `modelos/2n7002k_ps Rev B.lib`, `.SUBCKT 2N7002K 4 1 2` (D G S) | Vishay |
| F1 | Littelfuse 0456025.ER (LCSC C315877): 1.92 mΩ en frío, I²t fusión 45 A²s | resistencia 1.92 mΩ | Datasheet |
| DZ1, DZ2 | onsemi MM3Z12VT1G (LCSC C236177), 12 V SOD-323 | sin modelo de fabricante → zener `BZX84C12L` de `standard.dio` de LTspice con **`Cjo=1p`** (desviación declarada: la librería trae 150p) | Limitación declarada |
| D1–D3 | 1N5819 (BOM; antes B5819W) | sin modelo de fabricante → Schottky `1N5819` de `standard.dio` | Limitación declarada |
