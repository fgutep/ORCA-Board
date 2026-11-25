# 04 — Falla de la Placa 2 en las pruebas de carga y mejoras para proteger el LM5175

**Fecha:** 23 de noviembre de 2026
**Aplica a:** Placa 2 (falla) y diseño Rev 2 (mejoras).
**Complementa a:** [`simulacion/rev2/CAMBIOS_Mejoras_Rev2_borrador.md`](../../simulacion/rev2/CAMBIOS_Mejoras_Rev2_borrador.md), que cubre la protección **de sistema** (polaridad con Q5, fusible F1, snubber de hot-plug, interruptor de salida Q6). Este documento cubre el **entorno inmediato del LM5175**: lazos de VCC, bootstrap y gate. Las simulaciones de la Rev 2 no lo modelan porque no incluyen las inductancias del layout.

---

## 1. Qué pasó

Pruebas de carga con carga Rigol DL3021 y fuente EA PS 10060-170, controladas por [`pruebas_carga/carga.py`](../pruebas_carga/carga.py). El detalle por corrida (R01–R06) está en [`pruebas_carga/registro.md`](../pruebas_carga/registro.md).

- **Datos válidos** (Vin = 12 V, V_OUT = 12.025 V en vacío):

  | Iout | Vout | Pout | Eficiencia |
  |---|---|---|---|
  | 0.5 A | 11.96–11.99 V | 6 W | 71–72 % (Iin con resolución 0.1 A: ±10 %) |
  | 1.25 A | 11.935 V | 14.9 W | 89 % (±3.5 %) |

  La regulación de carga fue de 0.75 % entre vacío y 1.25 A. **No hay datos por encima de 15 W.**
- **Apagados en las cinco corridas con carga:** Iin → 0 con Vin firme en 12 V, medido en los terminales de la fuente. Ocurrieron unos 30–35 s después de aplicar la carga (R03, R04, R06) o al aplicar un escalón de 3.75 A (R05, R06). Al final la placa dejó de arrancar.
- **Conexión floja:** durante las pruebas se reparó una conexión floja; no se registró cuál. Una entrada que se abre produce la **misma firma** que un apagado del chip, así que parte de los primeros apagados puede deberse a ella. La caída de V_OUT con 0.5 A pasó de 137 mV (R01, ≈ 0.27 Ω) a 39 mV (R03, ≈ 0.08 Ω): el contacto cambiaba entre corridas.

## 2. Diagnóstico (multímetro, Vin = 20.84 V, sin carga)

| Medición | Esperado | Medido |
|---|---|---|
| VCC (C11) | ≈ 7.3 V | 40 mV |
| SS (C7) | ≈ 1.27 V | 4 mV |
| FB | 0.80 V | 0.2 mV |
| EN (pads de R2 y R3, lado EN) | ≈ 5.2 V (0.25·Vin + 0.18 V) | **0.61 V** |
| R3, lado VBUS | 20.84 V | 20.84 V |
| Caída en R1 (corriente del pin VIN) | — | 4 mV ≈ 0.4 mA (sin corto interno grueso) |
| R3 sin energía | 200 kΩ | 200 kΩ |
| EN–GNDA sin energía | ≈ 50–66 kΩ | 55 kΩ, subiendo (sano) |
| EN en modo diodo | Sano | Sano (este modo no detecta fugas resistivas de kΩ) |

El nodo EN solo tiene R3, R2 y el pin EN del chip, y en el esquemático no va a ningún conector. Los pasivos están sanos y, sin energía, no hay fuga. **Con energía, el pin EN hunde ≈ 90 µA** (equivale a ≈ 6.7 kΩ a GNDA) y queda por debajo del umbral de 1.23 V. **Conclusión: falla interna del LM5175.**

> **Actualización 24-nov-2026:** la placa volvió a funcionar al día siguiente sin intervención conocida. La falla de EN fue **transitoria**, así que no está confirmado que el chip tenga un daño permanente. Queda como fuga intermitente del nodo EN, de causa por determinar: térmica, humedad o contaminación bajo el chip, o un estado del propio LM5175. Las mejoras de §5 siguen aplicando, y la verificación de §6 pasa a hacerse con este mismo chip. Además se observó un rizado de ≈ 300 mV pico a pico en Vout con cualquier Vin del rango (ver [registro](../pruebas_carga/registro.md)).

> **Resultados de carga del 24-nov (R08–R22 en el [registro](../pruebas_carga/registro.md)):**
> - **Cortes en la zona de transición.** La placa deja de conmutar (Iin cae a 0 de golpe y el hiccup no re-arranca con la carga CC) solo cuando Vout/Vin está entre ≈ 1.0 y 1.2:
>   - 12 → 12 V: corta a ≈ 4.8 A (58 W).
>   - 22.2 → 25 V: corta entre 2.75 y 3 A (≈ 70 W), igual con escalones de 1 A que de 0.25 A.
>   - 6 → 7.2 V: aguanta 5 A durante 12 s y luego corta.
> - **Sin cortes fuera de esa zona.** En buck (16, 20 y 25.2 V) y en boost claro (9 → 12 V, 10.5 → 12 V, 18 → 25 V) no corta. Con 18 → 25 V llegó a **198.7 W** (η ≈ 87.6 %), con 124.8 W a η 91.2 %.
> - **Causa probable de los cortes:** el sensado de corriente o la compensación de pendiente en la transición (P6, P17). Se descartan el inductor, R8, R12 y los cables.
> - **Límite térmico.** Los MOSFET del lado de Vout (pierna de boost, Q3/Q4) son el punto caliente:
>   - 71 °C a 60 W en régimen (≈ 8.9 W de pérdidas).
>   - 142 °C y luego **180 °C** en ≈ 1 min de rampa de 150 a 200 W (≈ 28 W de pérdidas). Se cortó a mano, por encima de la Tj máx. de 175 °C.
>   - Sin disipador ni aire forzado, la potencia continua práctica ronda los **120 W** (P18).

Es el **segundo LM5175 del mismo pedido que falla**, y por otro pin: en la Placa 1 falló la etapa BOOT2/SW2 ([01](01_diagnostico_placa1.md)).

Hipótesis consistente con lo observado, no demostrada: una fuga en EN que crece con la temperatura hasta deshabilitar el chip. Explicaría los apagados a los ~30 s de carga y la progresión de intermitente a permanente.

## 3. Causas posibles

| # | Causa | Evidencia | Estado |
|---|---|---|---|
| 1 | **Sobretensión en pines de poco margen por el layout** (VCC, BOOT–SW, SW) | Componentes críticos a 16–29 mm del chip (§4). La Placa 1 falló justo en BOOT2–SW2 | Hipótesis principal. Se confirma con osciloscopio (§6) |
| 2 | Transitorios de la conexión floja o de hot-plug | La simulación S2 da 18.2 V de pico en VBUS a 12.6 V, lejos de los 60 V de VIN/EN/VISNS. El rebote del contacto no está simulado | Poco probable a 12 V; relevante a 25 V (37.6 V sin snubber) |
| 3 | Re-energizar varias veces con la falla presente (R04–R06 y el reinicio automático del script) | El daño pasó de intermitente a permanente | Agravante probable. Corregido en el script (§7) |
| 4 | ESD o manipulación al reparar la conexión o al medir | EN es de alta impedancia y su traza mide 28.8 mm | No verificable |
| 5 | Chip débil de origen | Dos fallas en el mismo pedido | No verificable |

**Poco probables:**
- **Temperatura:** ~2 W de pérdidas a 6–15 W, repartidos en la placa.
- **Escalones de carga:** están dentro del diseño, y la simulación S5 muestra el lazo estable con escalones de 1 ↔ 4 A.
- **Los 20.84 V:** se aplicaron después de la falla.

## 4. Layout fabricado contra el datasheet

Medido sobre `ORCA-BOARD.kicad_pcb` con pcbnew: distancia centro a centro hasta U1 y longitud total de traza por red. Los límites son del datasheet (§7.1, máximos absolutos), y el §11 pide estos componentes «cerca del IC». La regla del proyecto era menos de 10 mm.

| Elemento | Placa fabricada | Límite del pin | Riesgo |
|---|---|---|---|
| C11, bypass de VCC (1 µF) | 16.3 mm de U1; red VCC de 30.2 mm | VCC y LDRV: 8.5 V | Picos L·di/dt sobre VCC con cada pulso del gate drive |
| Bootstrap C3/C5 y D2/D3 | C3/C5 a ~18 mm; BOOT1 28.5 mm, BOOT2 29.4 mm | BOOT–SW: 8.5 V | C3/C5 se cargan a ≈ VCC − Vf ≈ 7 V. Si SW baja de unos −1.5 V en el *dead time*, D2/D3 los cargan por encima de 8.5 V |
| Gates | HDRV1 28.4, HDRV2 25.0, LDRV1 15.3, LDRV2 20.3 mm; Q1–Q4 a 17–22 mm | SW: −1 V (−3 V por 20 ns) | *Ringing* y picos negativos en SW1/SW2 |
| C1, bypass del pin VIN (100 nF) | 25.0 mm | VIN: 60 V | Filtrado pobre del pin VIN |
| EN (R2/R3) | R2 a 20.5 mm, R3 a 25.5 mm; traza de 28.8 mm; sin capacitor | EN: 60 V | Nodo de alta impedancia y largo que capta ruido |
| C17 (CS) y C18 (ISNS) | 11.1 y 14.0 mm; R9/R10 no toman Kelvin en R12 | CS/CSG: ±0.3 V | Ruido en el sensado de corriente |
| BIAS (pin 24) | Directo a VOUT | BIAS: 40 V | Sin filtro ante picos de salida |
| PGOOD (pin 17) | Sin conectar | — | No hay forma de ver una falla durante las pruebas |

## 5. Mejoras propuestas para la Rev 2 (entorno del LM5175)

Estado de todas: **PROPUESTA**, para integrar a la lista de cambios de la Rev 2. Los valores son puntos de partida y deben ajustarse con la medición de §6.

### Layout
| # | Cambio |
|---|---|
| P1 | Acercar U1 a Q1–Q4: trazas de gate drive, SW y BOOT por debajo de 10 mm |
| P2 | C11 a menos de 2 mm entre VCC (pin 23) y PGND (pin 22); subir a 2.2–4.7 µF X7R ≥ 16 V |
| P3 | C3/C5 y D2/D3 pegados a BOOT1/SW1 (pines 26/28) y BOOT2/SW2 (pines 20/18) |
| P4 | C1 junto al pin VIN (pin 2); subir a 1 µF |
| P5 | R2/R3 junto al pin EN (pin 1), más un C_EN de 1–10 nF a GNDA en el pin |
| P6 | C17 y C18 en los pines; R9/R10 tomados Kelvin desde los pads de R12 |
| P7 | Sacar SW1, SW2 y VBUS del plano de GND (In1.Cu) en el lazo rápido C_IN–Q_H–Q_L–R12 |
| P8 | Arreglo de vías térmicas en el pad expuesto |

### Protección de pines
| # | Cambio |
|---|---|
| P9 | Resistencia de 2.2–4.7 Ω en serie con D2 y con D3, para limitar la sobrecarga de C3/C5 |
| P10 | Huellas de resistencia de gate en HDRV1/2 y LDRV1/2 (montar 0 Ω; subir a 2–4.7 Ω si hay *ringing*) |
| P11 | Huellas de snubber RC en SW1 y SW2 a GND, sin montar; valores tras medir (por ejemplo 2.2 Ω + 1 nF de partida) |
| P12 | BIAS desde VOUT a través de un RC de ~10 Ω + 1 µF en vez de directo |
| P13 | Test point en PGOOD; la Rev 2 ya lo usa para Q7/Q6 |
| P14 | Test points rotulados en EN, VCC, SS, FB, COMP, BOOT1, BOOT2, SW1, SW2 y VOUT, para no medir nunca sobre las patas del chip |

### Margen de VCC
| # | Cambio |
|---|---|
| P15 | R1 de 10 Ω a 2.2–4.7 Ω, como advertía el [protocolo 02](02_protocolo_bringup.md) |
| P16 | Evaluar MOSFET de menor Qg en Q1–Q4: menos corriente de gate drive y menos calor en el chip, a cambio de algo más de Rds(on) |

### Zona de transición y térmica (pruebas del 24-nov)
| # | Mejora |
|---|---|
| P17 | Revisar la compensación de pendiente: C6 = 270 pF frente a ≈ 376 pF del *dead-beat* (Ec. 25, 4.7 µH y 5 mΩ). Probar el cambio en la Placa 2 junto con P6 y repetir el barrido de la zona de transición |
| P18 | Térmica de Q3/Q4 (lado de Vout): plano de cobre y vías térmicas bajo las pestañas en ambas caras, sitio para disipador o ventilador, y gates cortos (P1) para bajar las pérdidas de conmutación. Validar con un soak de 10 min a 120 W y foto térmica |

### Entrada y salida
Ya están cubiertas en el borrador de la Rev 2 (C6 Q5, C7 F1, C8 snubber, Q6). Como complemento o alternativa al snubber C8, evaluar un **TVS en VBUS** con clamp por debajo de 60 V (por ejemplo SMBJ28A si Vin ≤ 28 V), verificando que los capacitores de entrada soporten el clamp.

## 6. Verificación con el chip nuevo (antes de cargar)

**Bodges en la Placa 2 al cambiar el LM5175 (LCSC C113356):**
- Capacitor 0402 de 1 µF **directo entre los pines 22 (PGND) y 23 (VCC)**, que son adyacentes. Corrige el punto más débil sin rediseñar.
- 1–10 nF en paralelo con R2.

**Medición con osciloscopio** (TDS1012B, sondas ×10 con el menú en 10X y resorte de tierra corto; un cable de tierra largo crea un *ringing* falso). Medir sin carga y con escalones de 1 ↔ 4 A:

| Señal | Criterio |
|---|---|
| VCC en el pin (o en C11 / el capacitor nuevo) | Picos < 8.5 V |
| BOOT1–SW1 y BOOT2–SW2 (dos puntas, Math CH1 − CH2) | < 8.5 V |
| SW1 y SW2 contra GND | ≥ −1 V (≥ −3 V solo en picos ≤ 20 ns) |
| EN (multímetro) durante el soak | Estable en ≈ 0.25·Vin |

Si VCC o BOOT–SW pasan de 8.5 V, o SW baja de −3 V, **la causa 1 queda confirmada** y P1–P11 pasan a ser obligatorias.

## 7. Reglas de procedimiento

- **Conectar con la fuente apagada.** `carga.py` enciende la salida con una rampa de 0 a Vin en ~2 s.
- **Detenerse al primer apagado:** `--max-abortos 0` es ahora el valor por defecto. No re-energizar una placa con falla (lección 5 del [protocolo 02](02_protocolo_bringup.md)).
- **No medir sobre las patas del LM5175:** usar los pasivos listados en el protocolo o los test points de la Rev 2.
- **ESD:** pulsera o tocar tierra al manipular la placa o al retrabajar.
- **Contactos antes de cada sesión:** con 0.5 A, la caída de V_OUT en los terminales de la carga no debe pasar de ~40 mV (referencia de R03).
