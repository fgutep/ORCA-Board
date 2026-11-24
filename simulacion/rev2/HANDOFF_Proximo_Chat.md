# Handoff — Simulación Rev. 2 (LM5175): plan y faltante para el próximo chat

## ESTADO AL CIERRE (16-nov-2026, noche) — leer primero
- **Entregable:** `RESULTADOS_Sim_Rev2.md` (versión parcial, escrita con Opus). Cambios propuestos: `CAMBIOS_Mejoras_Rev2_borrador.md`.
- **Cerrado y válido:** S1 (Cjo=1p), S2 (Cjo=1p, precarga 1.0 V en remedios), S3 (logs con Cjo=150p: pendiente de re-correr con 1p), S5 (criterio propuesto, sin aprobar), S4a (referencia).
- **Falla:** S4b (2.16 % contra criterio 2 %; causa estimada; referencia no equivalente). S4c sin remedio (caída 19.0 %, valle I(L1) 31.9 A; `goff_c1`/`gon_c2` fallaron).
- **Pendiente:** S4c con remedio (proceso LTspice PID 32468 corriendo desde 18:20 del 16-nov; resultado en `S4_convertidor\S4cd_con.log` cuando termine). S4d no corrido. S4e 7.4→10 V y 16.8→7.2 V NO CONVERGEN; 22.2→24 V no corrido. Desviación: precarga 1 A en VOUT en S4c-e.
- **Decisiones de diseño (de Luis, en curso):** Q6 red de gate = diodo + Rrst 330 kΩ + R_G 22 kΩ (PROVISIONAL, apoyado en S3; requiere GAP de recierre ≥ 200 ms en firmware). C_carga externa a 25 V: 100–150 µF. Diodo de rueda libre montado. LEDs 0805 6.8 kΩ. NO comprometido: snubber de S2 (SOA de Q5 sin leer), polaridad invertida con C_IN cargado (−39.7 V, −431 A).
- **Pendiente de verificar:** C_IN 30 mΩ (S4a/S5) vs 31 mΩ del datasheet KEMET; SOA de Q5/Q6; criterio de S5; rampa de batería 20 µs (README dice 200 µs); PCB_Potencia_Mejoras_Rev2.md no está en el repo.
- **Próximo paso al retomar:** revisar `S4cd_con.log`; re-correr S3 con Cjo=1p; referencia S4a con la misma entrada; diseño de la red de gate de Q6 y protección de entrada.

**Mensaje para iniciar el chat nuevo (copiar y pegar):**

> Lee `simulacion/rev2/HANDOFF_Proximo_Chat.md`, `simulacion/rev2/ESTADO_PAUSA.md` y `simulacion/rev2/README_rev2.md`. Continúa la simulación de la Rev. 2 desde el paso 1 del plan. Orquesta con subagentes Sonnet para simular y usa Opus (como /advisor) solo para revisar resultados.

Documentos de referencia (en `C:\Users\Lenovo\Downloads\`): `PCB_Potencia_Sim_Rev2_Handoff.md` (casos y criterios S1–S5), `PCB_Potencia_Mejoras_Rev2.md` (circuito propuesto), `CargaLuis_ILmax_Handoff (2).md` (trampas del modelo LM5175; es la versión completa).

---

## 1. Reglas de trabajo (acordadas con Luis)

- **Hilo principal = orquestador.** No simula ni escribe bancos.
- **Workers = subagentes Sonnet** (`general-purpose`, `model: sonnet`): escriben `.cir`, corren LTspice, generan `resumen.md`.
- **Opus = revisor solo-lectura** (`model: opus`, vía `/advisor`): lee logs y resúmenes, da veredicto APROBADO / CON OBSERVACIONES / CORREGIR. Nunca simula.
- **Compuerta:** mostrar S1–S3 a Luis y esperar su OK antes de S4b–e.
- **Ahorro de tokens:** informes de workers ≤ 40–50 líneas, sin pegar netlists ni logs; los prompts remiten a `README_rev2.md` en vez de repetir reglas. Máximo 2–3 workers en paralelo.
- **LTspice en paralelo:** NUNCA matar `LTspice.exe` por nombre; solo el PID propio. Todo `.log` válido termina en "Total elapsed time"; si no, se repite la corrida.
- Batch: `"C:\Program Files\ADI\LTspice\LTspice.exe" -b <ruta absoluta>.cir`; todo `.cir` con `.options solver=alt method=modtrap`.

## 2. Hecho

| Ítem | Resultado | Archivo |
|---|---|---|
| Lib xorfix + batch + diferencias `.asc` | Verificado (xnor 1,0,0,1) | `README_rev2.md` |
| Piezas | Q1–Q4 BUK9Y3R5-40E · Q5/Q6 Vishay SiRS4401DP (DigiKey/Mouser) · Q7 2N7002K C81445 · F1 0456025.ER C315877 · zener MM3Z12VT1G C236177 (sin modelo → BZX84C12L de librería) | `README_rev2.md`, `modelos/` |
| S3 | Hecho + Fable (aprobado con observaciones) | `S3_salida/resumen.md` |
| S4a | Hecho + Fable (aprobado con observaciones) | `S4_convertidor/resumen_S4a.md` |

Detalle de números y observaciones de Fable: `ESTADO_PAUSA.md`.

## 3. Plan pendiente (en orden)

### Paso 1 — Seguimiento S3 (`S3f_*`), worker Sonnet
Antes: revisar si quedaron archivos `S3f_*` a medias en `S3_salida/` y reutilizarlos o borrarlos.
- **Tarea 1:** remedio de Q6 con encendido lento y apagado rápido: C_gd 100 nF **con diodo en serie** (señal de `standard.dio`, p. ej. 1N4148) + **R de reposición del nodo intermedio a source** (barrer 10 k / 47 k / 100 k), R_G 22 k. Medir: S3b a 25 V/8 A con L 1 mH y 0.5 µH (apagado ≈ ≤ 8 ms, SOA u < 0.7, V_DS < 32 V; ¿diodo de rueda libre pasa a obligatorio?), rejilla completa de `S3a_final`, recierre a 1/5/20 ms, S3c (V_GS a 6.8 V a 25/100 °C, P(Q6)). Opcional: seguidor NPN (colector a source, emisor a gate).
- **Tarea 2:** convertidor idealizado más realista: ILIM = 10 A, y ILIM = 10 A sin Rd en paralelo con Ls. Marcar qué combinaciones dejan de pasar (12 V/1000 µF, 25 V/470 µF).
- **Tarea 3:** SOA del encendido a 25 V con C_carga 100/150/220/330 µF (utilización + tiempo con V_DS > 15 V); límite de C_carga sin dos cifras significativas; R de precarga en paralelo con Q6.
- Salida: sección "§10 Seguimiento (revisión)" en `S3_salida/resumen.md`. Corregir de paso §8.3 (NPN, no PNP; "no abrir a 25 V/8 A" no es exigible).

### Paso 2 — S1 + S2, worker Sonnet (en paralelo con el paso 1)
- **S1:** ya corrieron S1a–S1d en `S1_polaridad/`. Verificar que cada `.log` esté completo; si no, repetir. Escribir `resumen.md` con tabla pasa/falla y tabla **P(Q5) vs V_IN a 25 y 100 °C** (punto crítico: 6 V, V_GS ≈ −6 V, 12 A; criterio ≤ 1 W).
- **S2:** solo existe `S2a_hotplug.cir`. Completar: cierre en 1 ms, `.step` R_bat 10/20/30 mΩ × L_cable 0.2/0.5/1 µH × BAT 12.6/25.2 V (18 combinaciones; antes, 3 pasos de prueba). Criterios: V(VBUS) pico < 32 V, I²t < 11.25 A²s (25 % de 45 A²s), |V_GS(Q5)| ≤ 15 V. ESR del polímero de C_IN: datasheet `Datasheets/KEMET_A4072_A759_condensadores.pdf`; si no se identifica, sensibilidad 15/30/60 mΩ. Si falla: snubber RC / más C con ESR, como caso aparte.

### Paso 3 — Revisión Opus de S1, S2 y S3f
Un solo agente Opus, solo lectura, informe ≤ 45 líneas.

### Paso 4 — Mostrar S1–S3 a Luis y esperar su OK
Decisiones que probablemente tendrá que tomar Luis:
- Remedio final de Q6 (red de gate) y si se agrega R de precarga.
- Límite de capacitancia de carga a 25 V (o cambiar de FET).
- Diodo de rueda libre en VOUT_EXT: montado o DNP.
- Resistencias de LED: 0805 o ≥ 6.8 kΩ.

### Paso 5 — S4b–e y S5, worker Sonnet (LM5175, C_SS 12 nF)
Partir de `S4_convertidor/S4a.cir`. Observaciones de Fable a aplicar: eventos tarde (≥ 3 ms; vout aún se asienta a 2.4 ms) y medir contra el valor previo al evento; rizado/caída/sobreimpulso con mediana de ~50 ns (picos numéricos); ISNS igual en todos los casos; `.meas` de valle de I(L1) posterior al arranque (criterio S4c < 10.6 A). Cada corrida ≈ 40 min: no más de 2 en paralelo.
- S4b: con Q5 (+R_bat, L_cable), bleeder y PGOOD; Q6 abierto. Diferencia < 2 % vs S4a; PGOOD al final del soft-start.
- S4c: cerrar Q6 contra 8 A + 470 µF, **con y sin el remedio elegido** (es el árbitro real de S3a). Caída < 9 % (ideal < 5 %), sin rearme, valle I(L1) < 10.6 A.
- S4d: abrir Q6 con 8 A: sobreimpulso < 7.5 % (ideal < 5 %).
- S4e: S4c/S4d en 7.4 → 10 V y 16.8 → 7.2 V (y 22.2 → 24 V si alcanza).
- S5: escalón 1 → 4 A y 4 → 1 A en 12 → 12 V, ESR C_OUT 12 mΩ: sobreimpulso, asentamiento, n.º de oscilaciones.
Luego revisión Opus.

### Paso 6 — Entregable final
`simulacion/rev2/RESULTADOS_Sim_Rev2.md`: tabla pasa/falla por caso con número medido, piezas con número LCSC/DigiKey, cambios de valor necesarios, lo no simulado y por qué (hiccup, térmica, zener/LED/Schottky genéricos, convertidor idealizado en S3), CSV de casos al límite, y lista de cambios para `PCB_Potencia_Mejoras_Rev2.md`. Revisión Opus final.

## 3b. /graphify pendiente (usar para ahorrar tokens; ejecutar con Sonnet)
Antes de los workers de los pasos 1–2, ejecutar `/graphify simulacion/rev2` con un subagente Sonnet para construir el grafo y consultar `graphify-out/` en vez de releer archivos completos. Excluir los datasheets de `modelos/` para abaratar la extracción.
Se pidió `/graphify simulacion/rev2`. Hechos los pasos 1–2 (graphify instalado; `graphify-out/` creado). Corpus: 37 archivos, ~49 k palabras (6 código, 16 docs, 15 PDF, casi todos datasheets de `modelos/`). Falta la extracción semántica (2 subagentes) y el armado del grafo; se pospuso por límite de uso. Sugerencia: correrlo al terminar el paso 6 (con los resúmenes finales), excluyendo los datasheets de `modelos/` para abaratarlo.

## 4. Cambios ya identificados para `PCB_Potencia_Mejoras_Rev2.md` (provisionales)
- Q6: agregar C_gd ~100 nF y R_G 22 k (no > ~27 k); falta cerrar la variante con apagado rápido (paso 1).
- Limitar C_carga a 25 V o agregar precarga (valor en el paso 1).
- Diodo de rueda libre en VOUT_EXT: footprint (DNP o montado según el paso 1).
- LED1/2/3: resistencias 0805 o ≥ 6.8 kΩ (≈ 0.12 W a 25 V con 4.7 k).
- Bleeder 1.5 kΩ en 2512 de 1 W: confirmado (0.42 W a 25 V; 25 → 5 V en 1.07 s).
- Q5/Q6 = Vishay SiRS4401DP (compra DigiKey/Mouser); Q7 = 2N7002K.
- Documentación KiCad: el símbolo de Q1–Q4 dice PSMN3R2-40YLD/C553274 pero la pieza montada es BUK9Y3R5-40E (LCSC C478247).
