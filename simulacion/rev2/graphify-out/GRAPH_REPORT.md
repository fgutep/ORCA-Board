# Graph Report - .  (2026-11-16)

## Corpus Check
- Corpus is ~44,684 words - fits in a single context window. You may not need a graph.

## Summary
- 262 nodes · 302 edges · 40 communities (32 shown, 8 thin omitted)
- Extraction: 93% EXTRACTED · 7% INFERRED · 0% AMBIGUOUS · INFERRED: 21 edges (avg confidence: 0.68)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Remedios Q6 y C1-C3|Remedios Q6 y C1-C3]]
- [[_COMMUNITY_S3 Resumen y Decisiones|S3 Resumen y Decisiones]]
- [[_COMMUNITY_Componentes y BOM|Componentes y BOM]]
- [[_COMMUNITY_S4a Linea Base|S4a Linea Base]]
- [[_COMMUNITY_S1 Polaridad Q5|S1 Polaridad Q5]]
- [[_COMMUNITY_S5 Escalon de Carga|S5 Escalon de Carga]]
- [[_COMMUNITY_s3_post.py|s3_post.py]]
- [[_COMMUNITY_S1 Hotplug y Snubber|S1 Hotplug y Snubber]]
- [[_COMMUNITY_S4b-e Convertidor Completo|S4b-e Convertidor Completo]]
- [[_COMMUNITY_S3f Tablas|S3f Tablas]]
- [[_COMMUNITY_meas y tabla_S4|meas y tabla_S4]]
- [[_COMMUNITY_S3f_post.py|S3f_post.py]]
- [[_COMMUNITY_raw2csv (S1S2)|raw2csv (S1/S2)]]
- [[_COMMUNITY_LEDs y Resistencias|LEDs y Resistencias]]
- [[_COMMUNITY_raw2csv (S2)|raw2csv (S2)]]
- [[_COMMUNITY_run.sh|run.sh]]
- [[_COMMUNITY_S3f_run2.sh|S3f_run2.sh]]
- [[_COMMUNITY_S3d Bleeder|S3d Bleeder]]
- [[_COMMUNITY_Cout y S5 Abierto|Cout y S5 Abierto]]
- [[_COMMUNITY_run_celdas.py|run_celdas.py]]
- [[_COMMUNITY_gen_S4.py|gen_S4.py]]
- [[_COMMUNITY_raw2csv_S4.py|raw2csv_S4.py]]
- [[_COMMUNITY_S5 postproceso.py|S5 postproceso.py]]
- [[_COMMUNITY_C_IN KEMET|C_IN KEMET]]
- [[_COMMUNITY_S2b rejilla snubber RC (20 cel|S2b rejilla snubber RC (20 cel]]

## God Nodes (most connected - your core abstractions)
1. `README Simulacion Rev2` - 17 edges
2. `S4 convertidor completo LM5175` - 11 edges
3. `ESTADO_PAUSA simulacion Rev2` - 10 edges
4. `HANDOFF proximo chat` - 10 edges
5. `C1-B C_gd + diodo + Rrst 330 k + R_G 22 k (apagado ~8 ms)` - 10 edges
6. `S4a linea base 12->12 V 8 A` - 9 edges
7. `RESULTADOS Sim Rev2 (parcial)` - 8 edges
8. `Q5 SiRS4401DP (proteccion de polaridad)` - 7 edges
9. `Veredicto S5: PASA ambos escalones (2.33 % / 2.26 %)` - 7 edges
10. `parse_log()` - 6 edges

## Surprising Connections (you probably didn't know these)
- `S4a resumen_S4a.md` --follows_rules_of--> `README Simulacion Rev2`  [INFERRED]
  C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/simulacion/rev2/S4_convertidor/resumen_S4a.md → README_rev2.md
- `Q1-Q4 BUK9Y3R5-40E (93E540)` --conceptually_related_to--> `S4 convertidor completo LM5175`  [INFERRED]
  ESTADO_PAUSA.md → C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/simulacion/rev2/S4_convertidor/resumen_S4a.md
- `S4b il_avg +2.16 % > 2 % FALLA` --conceptually_related_to--> `S4a linea base 12->12 V 8 A`  [INFERRED]
  RESULTADOS_Sim_Rev2.md → C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/simulacion/rev2/S4_convertidor/resumen_S4a.md
- `Modelo LM5175_TRANS_xorfix.lib` --implements--> `S4 convertidor completo LM5175`  [EXTRACTED]
  README_rev2.md → C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/simulacion/rev2/S4_convertidor/resumen_S4a.md
- `README Simulacion Rev2` --references--> `S4 convertidor completo LM5175`  [EXTRACTED]
  README_rev2.md → C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/simulacion/rev2/S4_convertidor/resumen_S4a.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Remedios candidatos de S3a (C_gd, RG, RC Q7) que dan el remedio elegido** — s3a_remedio_cgd, s3a_remedio_rg, s3a_remedio_q7rc, s3a_remedio_final [EXTRACTED 0.90]
- **Conflicto Rrst: V_GS (S3c), recierre y apagado rapido en la red con diodo** — s3f_seguimiento, s3f_rrst_vgs_conflicto, s3f_recierre_gap200, s3c_niveles_regimen [INFERRED 0.80]
- **Sub-bancos S1a-S1d que validan Q5** — s1a_directa, s1b_inversa, s1c_regeneracion, s1d_transitorio, s1_q5 [EXTRACTED 0.90]
- **Escalones de carga de S5 que sustentan el veredicto PASA** — s5_evento1_1a4, s5_evento2_4a1, s5_sin_oscilaciones, s5_criterio_propuesto, s5_veredicto_pasa [EXTRACTED 0.90]
- **S4b-e: convertidor real con Q5 entrada y Q6 salida confirma hallazgos de S1/S3** — s4b_montaje, s4b_resultado_s4b, s4c_sin_remedio, s4b_remedio_q6, s4cde_pendiente [INFERRED 0.80]
- **Red de gate de Q6 variante C1-B** — comp_cgd_100nf, comp_diode_serie_cgd, comp_rrst_330k, comp_rg_22k, comp_q6_sirs4401dp [EXTRACTED 0.95]
- **Compromiso snubber: tension baja vs corriente pico en Q5** — comp_snubber_rc_vbus, res_s2_pico_37v59, res_s2_snubber_28v29, res_s2_snubber_422a, open_soa_q5_10_100us [EXTRACTED 0.90]
- **S4c como arbitro de C1 (Q6 gate) y C_carga** — s4c_cierre_q6, chg_c1_q6_gate, chg_c3_cload_limit, res_s4c_sin_remedio_falla, open_s4c_con_remedio [INFERRED 0.80]
- **Conjunto documental Rev2 (estado, handoff, readme, resultados, cambios)** — doc_estado_pausa, doc_handoff, doc_readme_rev2, doc_resultados, doc_cambios_borrador, doc_resultados_borrador [INFERRED 0.85]
- **Evidencia de S3 sobre cierre/apertura de Q6** — s3a_cierre_q6, s3b_apertura_q6, s3c_vgs_q6, res_s3a_original_falla, res_soa_apagado_u188, res_recierre_200ms [INFERRED 0.75]

## Communities (40 total, 8 thin omitted)

### Community 0 - "Remedios Q6 y C1-C3"
Cohesion: 0.08
Nodes (29): C1 Q6 red de gate, C1-A C_gd directo + R_G 22 k (apagado lento), C1-B C_gd + diodo + Rrst 330 k + R_G 22 k (apagado ~8 ms), C2 diodo de rueda libre, C3 limite C_carga a 25 V (100-150 uF), C_gd 100 nF drain-gate de Q6, Diodo en serie con C_gd (1N4148 modelo), Diodo de rueda libre en VOUT_EXT (Schottky >= 40 V) (+21 more)

### Community 1 - "S3 Resumen y Decisiones"
Cohesion: 0.09
Nodes (27): S3 resumen: interruptor de salida Q6, PGOOD/Q7, LEDs, bleeder, Criterio PASA/AL LIMITE/FALLA con limite V_DS 32 V (umbral 25.6 V), S1b conexion inversa: bloquea (VBUS < 1 mV, fuga 2.9 nA) PASA, Convertidor idealizado (Ls||Rd, limitador 15 A, BWK), Decisiones pendientes para Luis (recierre, diodo, limite C, ILIM), Efectos del remedio: encendido 2.8-5 ms, apagado 65 ms/>100 ms, SOA apagado u=1.88, Seguidor NPN de apagado en gate (no simulado, pendiente), Regla de margen V_DS: limite 32 V, AL LIMITE > 25.6 V (+19 more)

### Community 2 - "Componentes y BOM"
Cohesion: 0.16
Nodes (22): C12 corregir simbolo/BOM KiCad de Q1-Q4, D1-D3 Schottky 1N5819 / B5819W, F1 Littelfuse 0456025.ER (fusible entrada), Q1-Q4 BUK9Y3R5-40E (93E540), Q7 2N7002K (PGOOD), Zener BZX84C12L Cjo=1p (modelo de MM3Z12VT1G), CAMBIOS Mejoras Rev2 (borrador), ESTADO_PAUSA simulacion Rev2 (+14 more)

### Community 3 - "S4a Linea Base"
Cohesion: 0.12
Nodes (18): C_OUT 440 uF / 12 mohm, C_SS 12 nF (real 120 nF), Modelo LM5175_TRANS_xorfix.lib, S4a resumen_S4a.md, S4a: vout 11.992 V, I(L1) 9.06 A prom, il_max 12.88 A, S4a: logs truncados por LTspice.exe matado externamente, S4a: PGOOD sube a 1.845 ms (fin soft-start), Abierto: rampa de bateria 20 us vs 200 us (Q5 pegado) (+10 more)

### Community 4 - "S1 Polaridad Q5"
Cohesion: 0.12
Nodes (18): S1 resumen: proteccion de polaridad Q5, C_IN 100 uF polimero (ESR 31 mohm) + 10 uF + 100 nF, F1 fusible 1.92 mohm, Q5 SiRS4401DP (proteccion de polaridad), S1 supuestos: sin rampa de BAT, sin conexion lenta, F1 constante, modelos genericos, S1c regeneracion a 5 A: PASA; pico 0.24 V informativo (L_cable x di/dt), S1d caso 3 (10 us): I(Q5) -431 A vs I_DM 350 A, FALLA informativo, Hallazgo: Q5 no bloquea barrido rapido con C_IN cargado (47 k x C_gs) (+10 more)

### Community 5 - "S5 Escalon de Carga"
Cohesion: 0.12
Nodes (18): S5 postproceso_salida.txt (salida de postproceso.py), S5 resumen: escalon de carga 1<->4 A, 12 V a 12 V, Carga: R7 12 ohm + fuente I1 +3 A, flancos 1 us (declarado), Criterio propuesto (pendiente de aprobacion): desviacion < 5 %, asentamiento < 1 ms, Evento 1->4 A: undershoot 2.33 % (279 mV), asentamiento 89 us, Evento 4->1 A: overshoot 2.26 % (271 mV), asentamiento 108 us, I(L1) baja a -0.24 A en el evento 4->1 A, S5.log valido: 7249.9 s, sin tolerance relaxed (+10 more)

### Community 6 - "s3_post.py"
Cohesion: 0.24
Nodes (16): a_rows(), a_table(), b_table(), c_table(), d_table(), decimate(), e_table(), _g() (+8 more)

### Community 7 - "S1 Hotplug y Snubber"
Cohesion: 0.14
Nodes (15): C8 snubber RC en VBUS, C9 polaridad invertida con C_IN cargado, Q5 SiRS4401DP (proteccion de polaridad), Snubber RC VBUS 470 uF + 0.10 ohm, Abierto: SOA de Q5 a 10-100 us sin leer, S1a |V_GS| 11.84 V, P(Q5) 0.387 W: PASA, S1b polaridad inversa en frio: PASA, S1d con C_IN cargado: -39.7 V, -431 A (FALLA informativo) (+7 more)

### Community 8 - "S4b-e Convertidor Completo"
Cohesion: 0.13
Nodes (15): S4b-e resumen: convertidor completo con Q5 entrada y Q6 salida, Tabla P(Q5) vs V_IN a 12 A (punto critico 6 V), Margen |V_GS| a 25.2 V es 21 % (11.84 V vs 15 V), apenas sobre umbral 20 %, Zener BZX84C12L con Cjo=1p (desviacion de libreria 150p), S1a conduccion directa: V_GS max 11.84 V, P<=0.387 W PASA, Zener Cjo=1p decidido, pero .inc de S3 aun con 150p (pendiente), Causa (estimacion): caida de VIN 0.22 V por R_bat+F1+Q5 sube corriente ~1.9 %, Desviacion S4c-e: precarga de 1 A en VOUT (rafaga inversa -8.5 A sin carga) (+7 more)

### Community 9 - "S3f Tablas"
Cohesion: 0.22
Nodes (4): precarga(), S3f: genera los bloques markdown de resumen.md seccion 10 desde los .log (solo ., soa25(), util()

### Community 10 - "meas y tabla_S4"
Cohesion: 0.22
Nodes (6): Lee los .meas de un .log de LTspice y valida el log. Uso: python meas.py S4b.log, read(), log(), Tabla pasa/falla de S4b-e a partir de los .log (via meas.py). Uso: python tabla_, v < crit PASA; margen < 20 % -> AL LIMITE; v >= crit FALLA (lo: umbral inferior, st()

### Community 11 - "S3f_post.py"
Cohesion: 0.47
Nodes (5): b_table(), eng(), S3f post-proceso: tablas desde .meas de los .log (importa s3_post, sin modificar, rec_rows(), rec_table()

### Community 12 - "raw2csv (S1/S2)"
Cohesion: 0.60
Nodes (4): export(), Exporta a csv/ formas de onda diezmadas (5000 pts, remuestreo uniforme en la ven, steps(), vpk()

### Community 13 - "LEDs y Resistencias"
Cohesion: 0.50
Nodes (4): C4 LED R en 0805 o >= 6.8 k, Resistencias LED1-3 (4.7 k), Decision LEDs: 0805 con 6.8 k, LED 4.7 k a 25 V disipa 114-120 mW (>100 mW 0603)

### Community 14 - "raw2csv (S2)"
Cohesion: 0.67
Nodes (3): export(), Exporta a csv/ formas de onda diezmadas (5000 pts, remuestreo uniforme en la ven, steps()

### Community 17 - "S3d Bleeder"
Cohesion: 0.67
Nodes (3): Bleeder 1.5 k 2512 de 1 W, S3d bleeder 1.5 k: 25->5 V en 1.07 s, 0.417 W PASA, S3e descarga de C_IN: 1 V tarda 14.8 s (informativo)

### Community 18 - "Cout y S5 Abierto"
Cohesion: 0.67
Nodes (3): C_OUT 440 uF / 12 mohm, Abierto: criterio de S5 propuesto sin aprobar, S5 1<->4 A: 2.33 %/2.26 %, ~0.1 ms, 0 oscilaciones

## Knowledge Gaps
- **72 isolated node(s):** `C_OUT 440 uF / 12 mohm`, `C_SS 12 nF (real 120 nF)`, `S4a: vout 11.992 V, I(L1) 9.06 A prom, il_max 12.88 A`, `S4a: PGOOD sube a 1.845 ms (fin soft-start)`, `S4a: logs truncados por LTspice.exe matado externamente` (+67 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **8 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `README Simulacion Rev2` connect `Componentes y BOM` to `Remedios Q6 y C1-C3`, `S4a Linea Base`, `S1 Hotplug y Snubber`?**
  _High betweenness centrality (0.132) - this node is a cross-community bridge._
- **Why does `S4 convertidor completo LM5175` connect `S4a Linea Base` to `S4b-e Convertidor Completo`, `Remedios Q6 y C1-C3`, `Componentes y BOM`, `S1 Polaridad Q5`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Why does `ESTADO_PAUSA simulacion Rev2` connect `Componentes y BOM` to `S3 Resumen y Decisiones`, `S1 Polaridad Q5`?**
  _High betweenness centrality (0.084) - this node is a cross-community bridge._
- **What connects `Exporta a csv/ formas de onda diezmadas (5000 pts, remuestreo uniforme en la ven`, `Exporta a csv/ formas de onda diezmadas (5000 pts, remuestreo uniforme en la ven`, `S3f post-proceso: tablas desde .meas de los .log (importa s3_post, sin modificar` to the rest of the system?**
  _84 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Remedios Q6 y C1-C3` be split into smaller, more focused modules?**
  _Cohesion score 0.08374384236453201 - nodes in this community are weakly interconnected._
- **Should `S3 Resumen y Decisiones` be split into smaller, more focused modules?**
  _Cohesion score 0.09116809116809117 - nodes in this community are weakly interconnected._
- **Should `S4a Linea Base` be split into smaller, more focused modules?**
  _Cohesion score 0.11764705882352941 - nodes in this community are weakly interconnected._