# 02 — Protocolo de bring-up por etapas

**Fecha:** 10 de noviembre de 2026
**Aplica a:** Placa 2 y a la Placa 1 tras cambiar el LM5175. Ver diagnóstico en [01](01_diagnostico_placa1.md).

---

## Datos del diseño que importan en banco

| Elemento | Valor / conexión | Consecuencia en pruebas |
|---|---|---|
| Controlador | LM5175PWPR (LCSC C113356), HTSSOP-28 | Pinout y footprint verificados contra datasheet: correctos |
| MOSFETs Q1–Q4 | **Nexperia BUK9Y3R5-40E** (marcado **93E540**, LCSC C553274), LFPAK56: pines 1–3 S, 4 G, pestaña D | Nivel lógico, V_GS máx ±10 V, Q_G ≈ 30 nC @5 V. Pinout coincide con footprint |
| Alimentación del pin VIN | VBUS → D1 (B5819W) → R1 10 Ω → pin 2, C1 100 nF | Caída en R1 / 10 Ω = corriente del regulador de VCC (medición muy útil) |
| VISNS (pin 3) | Directo a VBUS (etiqueta "VIN" en el esquemático) | No confundir con el pin 2 al medir |
| EN/UVLO | R3 200 k (VIN) / R2 66.5 k (GNDA) → EN ≈ 0.25·V_IN | Arranque en V_IN ≈ 4.93 V. Al superar 1.23 V el chip inyecta 3.5 µA → EN se ve ~0.18 V más alto |
| VCC | C11 1 µF a GND. Regulación 7.35 V típ., UVLO 3.27 V, límite ≥ 65 mA | VCC clavado en ~3.3 V = sobrecarga / UVLO |
| BIAS | Unido a VOUT, C10 100 nF | VCC puede "colarse" a VOUT por dentro del chip (VOUT ≈ VCC − 0.3 V si no conmuta) |
| Bootstrap | D2 (VCC→BOOT1) + C3 100 nF; D3 (VCC→BOOT2) + C5 100 nF | BOOT–SW debe medir alto y subir (carga de C) sin energía |
| MODE | R4 93.1 k → **CCM, hiccup habilitado** | |
| RT | R5 71.5 k → 350 kHz | Período ≈ 2.86 µs |
| SS | C7 120 nF, 5 µA → rampa ≈ 19 ms hasta 0.8 V, clamp 1.27 V | SS ≈ 0 permanente = el chip nunca arranca |
| FB | R13 49.9 k (VOUT) / R14 1.65 k + RV1 5 k (reóstato, pin 2 y 3 a GNDA) | V_OUT ajustable **≈ 6.8 V (RV1 = 5 k) a 25.0 V (RV1 = 0)** |
| GNDA–GND | R15 0 Ω (JP1) | Debe medir ~0 Ω |

Puntos de medición recomendados (nunca en las patas del LM5175, pitch 0.65 mm):
- **GND:** pata negativa de C2 o negativo del XT60 de entrada.
- **VCC:** pads de C11. **EN:** pads de R2. **BOOT1/BOOT2:** cátodo de D2 / D3.
- **SW1:** drain (pestaña) de Q2. **SW2:** drain (pestaña) de Q4. **Gates:** pin 4 de cada LFPAK.

---
---

## Errores a evitar (lecciones de la Placa 1)

1. **Cables y caimanes:** revisar contacto y polaridad antes de cada encendido. Medir V_IN **en la placa**, no en la fuente.
2. **Siempre con límite de corriente** y sin carga hasta completar la etapa 4.
3. **No medir sobre las patas del LM5175** con la placa energizada; usar los pasivos listados arriba.
4. **Osciloscopio TDS1012B:** si la punta está en ×10, poner **CHx MENU → Sonda → 10X**; si no, todo se ve 10 veces más pequeño y se pierde el disparo ("Auto" en lugar de "Trig'd").
5. **No dejar la placa energizada a V_IN alto con una falla activa** (a 14 V el LM5175 disiparía ~1 W).
6. **ESD:** manipular la placa y el chip con pulsera o tocando tierra antes.
7. **Avanzar por etapas:** no pasar a la siguiente sin cumplir los criterios de la anterior.

---

## Protocolo de arranque

### Etapa 0 — Inspección visual (lupa)

**Placa 2 (10-nov-2026):** ✅ todos los puntos OK.

- [ ] Punto de pin 1 del LM5175 coincide con la serigrafía; marca "LM5175" legible.
- [ ] Sin puentes en los 28 pines; soldadura completa en ambos lados.
- [ ] D1–D3 (SOD-123) con la raya del lado del cátodo según serigrafía.
- [ ] Polaridad de C2, C19, C20 (polímeros radiales).
- [ ] R15 (0 Ω, GNDA–GND) montada.
- [ ] Q1–Q4 marcados 93E540 y orientados (4 patitas hacia el lado de la serigrafía correspondiente).
- [ ] Sin componentes faltantes ni levantados (C3, C5, C11, R1, R2, R3, R13, R14, RV1).

### Etapa 1 — Mediciones sin energía (multímetro)

Descargar capacitores antes. En resistencia, esperar 5–10 s por lectura.

| # | Medición | Roja | Negra | Criterio OK | Placa 2 |
|---|---|---|---|---|---|
| 1 | VIN–GND | VBUS | GND | No corto; sube mientras carga | |
| 2 | VOUT–GND | VOUT (J2+) | GND | Sube hasta ~56 kΩ | |
| 3 | VCC–GND | Pad VCC de C11 | GND | Alto, subiendo (no decenas de Ω) | |
| 4 | **BOOT1–SW1** | Cátodo D2 | Drain Q2 | **Alto, subiendo** | |
| 5 | **BOOT2–SW2** | Cátodo D3 | Drain Q4 | **Alto, subiendo — NO ~28 Ω** | |
| 6 | GNDA–GND | Pad GNDA de R2 | GND | ~0 Ω | |
| 7 | Gate–Source Q1 | Gate Q1 | Source Q1 | kΩ–MΩ subiendo | |
| 8 | Gate–Source Q2 | Gate Q2 | Source Q2 | kΩ–MΩ subiendo | |
| 9 | Gate–Source Q3 | Gate Q3 | Source Q3 | kΩ–MΩ subiendo | |
| 10 | Gate–Source Q4 | Gate Q4 | Source Q4 | kΩ–MΩ subiendo | |
| 11 | D1, D2, D3 (modo diodo) | Ánodo | Cátodo | ~0.2–0.4 V; invertido OL | |
| 12 | RV1 pin 1 – GNDA (girando) | RV1 pin 1 | GNDA | Varía 0 ↔ 5 kΩ en 25 vueltas | |

**Placa 2 (10-nov-2026):** ✅ las 12 mediciones cumplen el criterio; BOOT1–SW1 y BOOT2–SW2 altos y subiendo (sin la falla de la Placa 1).

**Ajuste previo:** dejar RV1 en **máxima resistencia (~5 kΩ)** → V_OUT esperado ≈ 6.8 V.

Si las filas 4 o 5 dan un valor bajo y fijo: **no energizar**; es la misma falla de la Placa 1.

### Etapa 2 — Energizado con EN forzado a 0 (shutdown)

- Fuente 7.2 V, límite **100 mA**, sin carga. Pinzas metálicas cortocircuitando los dos pads de R2.

| Medición | Criterio OK | Placa 2 |
|---|---|---|
| EN (pad R2 – GNDA) | < 0.4 V | |
| Corriente de fuente | µA / ~0 mA | |
| VCC (C11) | ~0 V | |

### Etapa 3 — Standby (regulador de VCC sin PWM)

- Fuente **4.5 V**, límite 100 mA, sin carga, sin pinzas.

| Medición | Criterio OK | Placa 2 |
|---|---|---|
| Corriente | ~1–2 mA | |
| VCC | ≈ 4.2–4.4 V | |
| EN | ≈ 1.1–1.3 V | |
| SS | ≈ 0 V | |

Nota: en la Placa 1 esta etapa salió normal; la falla aparecía al habilitar.

### Etapa 4 — Primer arranque

- Fuente **7.2 V**, límite **300 mA**, sin carga, RV1 en máxima resistencia. Osciloscopio listo: CH1 SW1 (drain Q2), CH2 VCC (C11), sondas ×10 con menú en 10X.

| Medición | Criterio OK | Criterio de PARADA | Placa 2 |
|---|---|---|---|
| Corriente | Decenas de mA hasta ~200 mA (gate drive ~50 mA + pérdidas en CCM forzado) | Fuente en CC con VCC bajo | |
| Caída en R1 | Proporcional a I_VCC (≤ ~0.6 V) | ≥ 0.8 V con VCC ≈ 3.3 V | |
| VCC | ≈ 5.5–7 V estable | ≈ 3.3 V o diente de sierra | |
| SS | Rampa (~19 ms) y se queda arriba (~1.27 V) | Se queda en mV | |
| FB (en R14/R13, contra GNDA) | **0.80 V** fijo | Distinto de 0.8 V | |
| VOUT | **≈ 6.8 V** | ≈ 3 V o sigue a VCC | |
| SW1 / SW2 (osciloscopio) | Ondas cuadradas continuas, período ≈ 2.86 µs (zona de transición: ambos nodos conmutan) | Ráfagas aisladas cada decenas de µs | |
| Temperatura LM5175 | Tibio | Caliente sin carga | |

Si aparece cualquier criterio de PARADA: apagar, repetir filas 4–5 de la Etapa 1.

Margen de VCC a vigilar: con BUK9Y3R5-40E, el gate drive en la zona de transición puede pedir ~50–55 mA, cerca del límite del regulador (≥ 65 mA), y R1 (10 Ω) + D1 restan ~0.9 V al pin VIN. Si VCC queda bajo (< ~5 V) operando, considerar bajar R1 a ~2.2 Ω.

### Etapa 5 — Ajuste de salida (sin carga, V_IN = 7.2 V)

- Girar RV1 hacia menor resistencia por pasos; **no superar V_OUT ≈ 10 V** (regla V_OUT ≤ 1.4 × V_IN).

| RV1 (pin 1–GNDA) | V_OUT esperado | V_OUT medido | FB |
|---|---|---|---|
| 5.0 kΩ | 6.80 V | | |
| 4.5 kΩ | 7.29 V | | |
| 4.0 kΩ | 7.87 V | | |
| 3.5 kΩ | 8.55 V | | |
| 3.0 kΩ | 9.38 V | | |
| 2.7 kΩ | 9.98 V (límite a 7.2 V de entrada) | | |

(V_OUT = 0.8 × (1 + 49.9 k / (1.65 k + RV1)))

### Etapa 6 — Barrido de entrada (sin carga)

- Probar V_IN = 6, 7.2, 9, 12, 16.8 V (subir el límite según haga falta), con V_OUT fijado y respetando V_OUT ≤ 1.4 × V_IN.
- Verificar los modos: boost (V_IN < V_OUT, conmuta SW2), transición (ambos), buck (V_IN > V_OUT, conmuta SW1).

### Etapa 7 — Pruebas con carga

- Conexiones cortas y gruesas por XT60; medir V_IN y V_OUT **en los terminales de la placa**.
- Escalones: 0.5, 1, 2, 4 A (luego 8 A continuo) vigilando temperatura de Q1–Q4, L1 y LM5175, ripple de salida y eficiencia.

| V_IN | I_OUT | V_OUT | I_IN | Eficiencia | T máx (componente) | Ripple |
|---|---|---|---|---|---|---|
| | | | | | | |

