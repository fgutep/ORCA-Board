# Resultados de simulación Rev. 2 — PCB de Potencia LM5175

Fuentes: `resumen.md` de S1, S2, S3 (incl. §8 y §10) y S5; `S4_convertidor\resumen_S4a.md`, `resumen_S4b_e.md` y `S4c_sin.log`; `README_rev2.md`; `ESTADO_PAUSA.md`; borradores `RESULTADOS_Sim_Rev2_borrador.md` y `CAMBIOS_Mejoras_Rev2_borrador.md`. Ningún número se recalculó; lo que no sale de un `.meas` o de un resumen va como "(estimación)" o "pendiente".

## 1. Resumen ejecutivo

- **Q5 (polaridad)** pasa en operación normal y en conexión inversa en frío (S1a-c). Con C_IN cargado y la batería invertida en caliente (S1d) no bloquea: −39.7 V y −431 A en el caso abrupto. Sin solución cerrada.
- **Hot-plug a 25.2 V (S2)**: sin snubber, el pico de V(VBUS) llega a 37.59 V (límite 32 V). El snubber 470 µF + 0.10 Ω arregla la tensión, pero sube la corriente hasta 422.4 A (> I_DM 350 A). No se adopta hasta leer la SOA de Q5.
- **Q6 (salida)**: el circuito original falla al cerrar contra carga capacitiva, tanto en S3 (caída hasta 20.5 %) como en el convertidor real (S4c sin remedio: 19.0 %, valle de I(L1) 31.9 A). La red con diodo + Rrst 330 kΩ pasa en S3 (convertidor idealizado). S4c con remedio no tiene resultado: la decisión es **provisional**.
- **Convertidor (S4a, S5)**: regula 12 V a 8 A y responde a 1↔4 A con ~2.3 % y ~0.1 ms. S4b falla el criterio < 2 % por la entrada no ideal (estimación). S4d no se corrió. En S4e, dos casos no convergen y uno no se corrió.
- Abierto: S4c con remedio, S3 re-corrido con Cjo=1p, SOA de Q5, aprobación del criterio de S5 y la polaridad invertida con C_IN cargado.

## 2. Alcance, modelos y reglas

- LTspice 26.0.2, `.options solver=alt method=modtrap`. LM5175 con `LM5175_TRANS_xorfix.lib`. El hiccup no se puede simular.
- Piezas modeladas con los modelos de fabricante de `modelos/`: Q1–Q4 BUK9Y3R5-40E, Q5/Q6 SiRS4401DP y Q7 2N7002K. El zener, los LED, los Schottky y el 1N4148 son genéricos de `standard.dio`.
- **Zener:** `BZX84C12L` con **`Cjo=1p`** (decisión del proyecto; la librería trae 150p). Excepción: los `.log` de S3 usan 150p (ver §7).
- **Regla de margen:** FALLA si no cumple; AL LIMITE si el margen es < 20 % del límite; PASA en otro caso.
- **V_DS:** el límite de diseño es 32 V (40 V es el máximo absoluto). AL LIMITE si V_DS > 25.6 V.
- **SOA de Q6 (S3):** se usa la utilización u. u ≤ 0.7 PASA, 0.7–1.0 AL LIMITE y > 1 FALLA. Las curvas del datasheet están digitalizadas (±10 %), con TC = 25 °C y sin Zth.
- **S3 usa un convertidor idealizado:** fuente + limitador de 15 A + Ls ‖ Rd. No tiene soft-start ni límite por ciclo. Es optimista: el ILIM real es ~10 A.

## 3. Pasa/falla por banco

### S1 — Polaridad (Q5)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| S1a V_GS | \|V_GS\| ≤ 15 V | 11.84 V (25.2 V) | PASA (margen 21 %) |
| S1a disipación a 12 A | P ≤ 1 W | 0.387 W (6 V, 100 °C) | PASA |
| S1b inversa en frío, −25.2 V | \|V(VBUS)\| < 0.5 V; fuga < 1 mA; V_DS < 32 V | 0.532 mV; 0.426 µA (100 °C); 25.20 V | PASA |
| S1c regeneración 5 A | V(VBUS)−V(BAT) < 0.2 V | 0.023–0.024 V | PASA |
| S1c flanco de 5 A en 5 µs | idem | 0.240 V | FALLA (informativo) |
| S1d inversión con C_IN cargado (casos 1/2) | \|V(VBUS)\| < 0.5 V | −25.38 V / −13.03 V | FALLA (informativo) |
| S1d caso 3 (+25.2 → −25.2 V en 10 µs) | idem; I_DM 350 A | −39.7 V; −431 A; 314 W pico | FALLA (informativo) |
| S1d remedio R_gate (47 k → 1 k) | idem | −25.4 V … +0.59 V; con 1 k V_DS 25.81 V | Sin remedio práctico |

### S2 — Hot-plug y F1

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| 25.2 V, 9 casos (R_bat × L_cable) | V(VBUS) < 32 V | 25.98–37.59 V (5 FALLA, 4 AL LIMITE) | FALLA |
| 12.6 V, 8 casos | idem | ≤ 18.22 V | PASA |
| I²t de F1 | < 11.25 A²s | 0.81 A²s | PASA |
| V_DS inverso de Q5 | < 32 V | 25.68 V (máx.) | AL LIMITE (2 casos) |
| Pico de corriente | < I_DM 350 A | 328.9 A | AL LIMITE |
| Sensibilidad a la ESR del polímero (15/60 mΩ) | V(VBUS) < 32 V | 34.83–40.41 V / 29.20–34.02 V | FALLA |
| Remedio: N × 100 µF extra | idem | 35.99–37.00 V | FALLA |
| Remedio: snubber 470 µF + 0.10 Ω (precarga 1.0 V) | idem | ≤ 28.29 V (25.2 V); ≤ 13.62 V (12.6 V) | AL LIMITE (margen 11.6 %) |
| Snubber: pico de corriente | < I_DM 350 A | hasta 422.4 A | FALLA (3 de 18); SOA pendiente |

### S3 — Salida (Q6, PGOOD, bleeder): convertidor idealizado, zener Cjo=150p

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| S3a original (R_G 10 k), 9 casos | caída ≤ 5 %, PGOOD no cae, sin reconmutación | caída 9.0–20.5 %; PGOOD cae 9/9; reconmuta 4/9 | FALLA |
| S3a C_gd 100 nF directo + R_G 22 k | idem | 9/9; caída máx. 2.19 % | PASA |
| S3a diodo + Rrst (47/100/330 k), 27 casos | idem | 27/27; caída máx. 4.36 % | PASA |
| S3a recierre, Rrst 330 k | pasan todos | GAP 20 ms 0/6; 100 ms 3/4; ≥ 200 ms 4/4 | PASA solo con GAP ≥ 200 ms |
| S3a con ILIM 10 A | idem | 25 V/1000 µF: caída 9 % | FALLA en ese caso |
| SOA del encendido a 25 V | u ≤ 0.7 | 100 µF 0.576; 150 µF 0.717; 220 µF 0.96; 330 µF 1.37; 470 µF 1.79 | FALLA con ≥ 330 µF |
| S3b sin diodo de rueda libre, 25 V/1 mH | V_DS < 32 V | 30.73 V; u 0.716 | AL LIMITE |
| S3b con Schottky | idem | 25.5 V; u 0.66 | PASA |
| S3b C_gd directo, apagado 25 V/8 A | SOA | u = 1.88 | FALLA |
| S3b diodo + Rrst (≥ 47 k) | apagado ~8 ms | 7.94–8.19 ms; u 0.22 (0.5 µH) | PASA |
| S3c V_GS a 6.8 V, 100 °C, Rrst | \|V_GS\| ≥ 5 V | 100 k −4.67 V; 220 k −5.07 V; 330 k −5.19 V | PASA solo con ≥ 220 k (margen nominal) |
| S3c P(Q6) | ≤ 0.5 W | ≤ 0.172 W | PASA |
| S3d bleeder 1.5 k | 25 → 5 V en ~1.1 s | 1.07 s; 0.417 W | PASA |
| LED con 4.7 k a 25 V | P ≤ 0.1 W (0603) | 114–120 mW | FALLA en 0603 |

### S4 — Convertidor completo (parcial)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| S4a 12 → 12 V, 8 A | referencia | vout 11.992 V; I(L1) prom./pico/valle 9.062/9.902/8.245 A; t_pgood 1.845 ms | Hecho (referencia) |
| S4b: Q5 + R_bat + L_cable, Q6 abierto (`S4b_zA.log`) | diferencia < 2 % vs S4a | il_avg +2.16 %; il_pk +3.14 %; il_vy +1.46 %; vout +0.02 % | **FALLA** (il_avg, il_pk) |
| S4b PGOOD | sube al final del soft-start | 1.790 ms (vout 11 V a 1.818 ms) | PASA |
| S4c sin remedio: cierre de Q6 contra 8 A + 470 µF (`S4c_sin.log`) | caída < 9 % | vpre_c 12.048 V; vmin_c 9.759 V; caída 18.996 % | **FALLA** |
| S4c sin remedio | valle de I(L1) < 10.6 A | vyl_max_c 31.879 A | **FALLA** |
| S4c sin remedio | informativo | ipk_q6 59.714 A; `goff_c1` y `gon_c2` FALLARON (sin tiempos) | — |
| S4c con remedio | caída < 9 %, valle < 10.6 A | en curso, sin resultado | **PENDIENTE** |
| S4d apertura de Q6 con 8 A | sobreimpulso < 7.5 % | no corrido | PENDIENTE |
| S4e 7.4 → 10 V y 16.8 → 7.2 V | idem S4c/S4d | no converge (incluido el reintento `r5`) | PENDIENTE |
| S4e 22.2 → 24 V | idem | no corrido | PENDIENTE |

La causa probable de la falla de S4b es la entrada no ideal. VIN cae 0.22 V (11.781 V) y eso sube la corriente de entrada ~1.9 % (estimación). Por eso S4a no es una referencia equivalente.

### S5 — Escalón de carga 1 ↔ 4 A, 12 → 12 V (criterio propuesto, sin aprobar)

| Caso | Criterio | Medido | Estado |
|---|---|---|---|
| 1 → 4 A | desviación < 5 %; asentamiento ±2 % < 1 ms | 2.33 % (279 mV); 89 µs; 0 oscilaciones | PASA |
| 4 → 1 A | idem | 2.26 % (271 mV); 108 µs; 0 oscilaciones | PASA |

## 4. Decisiones recomendadas

| Decisión | Evidencia | Estado |
|---|---|---|
| Q6: red de gate con C_gd 100 nF + diodo en serie + Rrst 330 kΩ + R_G 22 kΩ, con recierre ≥ 200 ms | S3a 27/27 (caída ≤ 4.36 %), apagado ~8 ms, S3c −5.19 V. S4c sin remedio confirma que el original falla (19.0 %) | **PROVISIONAL**: apoyada en S3 (idealizado, Cjo=150p). Falta S4c con remedio |
| Límite de C_carga a 25 V: ≤ 100 µF (150 µF queda AL LIMITE) | SOA del encendido, u 0.576 / 0.717 | PROVISIONAL (incertidumbre ±30–40 % supuesta) |
| Diodo de rueda libre en VOUT_EXT (Schottky ≥ 40 V) | S3b: 30.73 → 25.5 V | PROVISIONAL (huella; montar según la carga) |
| Resistencias de LED: 0805, o ≥ 6.8 k | S3: 114–120 mW en 0603 | APROBADO POR SIMULACIÓN |
| Bleeder 1.5 k en 2512 | S3d: 0.417 W, 1.07 s | APROBADO POR SIMULACIÓN |
| F1 0456025.ER sin cambio | S2: I²t 0.81 A²s (2.49 A²s con snubber) | APROBADO POR SIMULACIÓN |
| Q5 SiRS4401DP en operación normal | S1a-c | APROBADO POR SIMULACIÓN |
| Snubber de S2 (470 µF + 0.10 Ω) | Tensión ≤ 28.29 V; corriente hasta 422.4 A > I_DM | **NO COMPROMETIDO**: falta la SOA de Q5 a 10–100 µs |
| Polaridad invertida con C_IN cargado | S1d: −39.7 V, −431 A; R_gate no basta | **ABIERTO** (¿escenario real? Solución de sistema sin simular) |
| C_OUT 440 µF / lazo sin cambio | S5 pasa | PENDIENTE de aprobar el criterio de S5 |

## 5. Lo no simulado y por qué

| Tema | Por qué |
|---|---|
| Hiccup del LM5175 | El modelo de TI no lo reproduce. El recierre seguro de Q6 (≥ 200 ms) depende de él. |
| S4c con remedio, S4d, S4e 22.2 → 24 V | Corrida en curso o sin lanzar al cierre. |
| S4e 7.4 → 10 V y 16.8 → 7.2 V | No convergen. |
| Térmica | Sin autocalentamiento: solo temperatura de unión impuesta (25/100 °C) y SOA a TC = 25 °C. La fusión de F1 no se simula (solo I²t). |
| Piezas reales de zener, LED, Schottky y 1N4148 | No hay modelo del fabricante; se usan los genéricos de `standard.dio`. |
| Solución de sistema para la polaridad invertida con C_IN cargado | No se propuso un circuito concreto. |
| Rebote del XT60, ESL y derating por DC bias de los cerámicos | Sin modelar. |
| Tolerancias de R y zener en V_GS de Q6 a 6.8 V | El margen de 0.19 V es nominal. |
| Seguidor NPN de apagado de Q6 | Opcional; no se simuló. |

## 6. Piezas

| Ref | Pieza | Número de compra |
|---|---|---|
| Q1–Q4 | Nexperia BUK9Y3R5-40E | pendiente de verificar |
| Q5, Q6 | Vishay SiRS4401DP-T1-GE3 | DigiKey/Mouser; número de catálogo pendiente de verificar |
| Q7 | Vishay 2N7002K-T1-GE3 | LCSC C81445 |
| F1 | Littelfuse 0456025.ER | LCSC C315877 |
| DZ1, DZ2 | onsemi MM3Z12VT1G | LCSC C236177 |
| D1–D3 | 1N5819 (BOM; antes B5819W) | pendiente de verificar |
| C_IN, snubber, C_gd, diodo serie, rueda libre, L1, R de LED | — | pendiente de verificar |

## 7. Inconsistencias y limitaciones

1. Los `.log` de S3 se corrieron con `Cjo=150p` (`common_S3.inc`), pero el proyecto decidió 1p. Hay que re-correrlos; el efecto no está medido.
2. S1 y S2 usan el zener con Cjo=1p. Los remedios de S2 parten de una precarga V(vbus) = 1.0 V, lo que los hace hasta ~0.5 V optimistas.
3. La rampa de batería de S4b es de 20 µs (el README pide 200 µs). En las pruebas, con rampas ≥ 100 µs la compuerta de Q5 queda pegada a VIN (Q5 apagado). Hallazgo abierto.
4. S4a y S5 usan C_IN con 30 mΩ sin respaldo de datasheet. S1, S2 y S3 usan 31 mΩ (datasheet KEMET A759).
5. La SOA de Q5/Q6 no se leyó de forma fiable. S2 compara solo contra I_DM (curva ilegible) y S3 digitalizó la curva para Q6.
6. El criterio de S5 es propuesto y está sin aprobar.
7. Polaridad invertida con C_IN cargado (S1d): −39.7 V y −431 A en el caso abrupto. Sin solución cerrada.
8. S4c-e corren con una precarga de 1 A en VOUT (sin ella, el solver se atasca); es una desviación del enunciado.
9. D1–D3: el README y la BOM dicen 1N5819, y el borrador anterior decía B5819W.

## 8. Próximos pasos mínimos

1. Terminar S4c con remedio y cerrar la decisión de Q6.
2. Re-correr S3 con zener Cjo=1p.
3. Correr una referencia S4a con la misma entrada que S4b (R_bat, L_cable, F1, Q5) y repetir la comparación < 2 %.
4. Lograr la convergencia de S4e (7.4 → 10 V, 16.8 → 7.2 V) y correr 22.2 → 24 V y S4d.
5. Leer la SOA de Q5 a 10–100 µs y decidir el snubber de S2.
6. Aprobar (o cambiar) el criterio de S5.
