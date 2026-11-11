# 01 — Diagnóstico de la Placa 1

**Fecha:** 10 de noviembre de 2026
**Método:** fuente con límite de corriente, multímetro, inyección de corriente, osciloscopio TDS1012B y cámara térmica Keysight U5855A.

## Síntoma

A 7.2 V de entrada la salida quedaba en ~3 V y el trimmer RV1 no tenía efecto. La falla existía **antes** de un incidente posterior con los cables, que por tanto no es la causa original.

## Mediciones (fuente sin carga)

| V_IN | Corriente | VCC | VOUT | SS |
|---|---|---|---|---|
| 4.5 V | 1 mA | 4.30 V | 3.70 V | 1 mV |
| 5.0 V | 75 mA | 3.29 V | 2.91 V | 1.3 mV |
| 6.0 V | 89 mA | 3.31 V | 2.99 V | 2.0 mV |
| 7.0 V | 93 mA | 3.30 V | 3.01 V | 2.7 mV |
| 7.5 V | 93 mA | 3.30 V | 3.00 V | 2.8 mV |

- Caída en R1 ≈ 0.8 V → ~80 mA entrando al regulador de VCC. Solo se calienta el LM5175.
- EN a GNDA → consumo cae a ~0: el chip responde al enable y el regulador de VCC está sano en standby.
- Gates de Q1–Q4, VOUT–GND (~56 kΩ), divisor de FB y D1–D3: sanos.
- **BOOT2–SW2 = 28 Ω** con multímetro (BOOT1–SW1 sano), igual al invertir puntas y **también con C5 retirado**.
- Inyección BOOT2(+) → SW2(−): **0.8 V a 100 mA** (no óhmico → unión de silicio). Se calienta el LM5175.
- Con energía: LDRV1 ≈ 2.7 V (Q2 encendido), BOOT2–SW2 = 2.61 V → 2.61 V / 28 Ω ≈ 93 mA = consumo de la fuente.
- Carga de 10 kΩ en VOUT: 2.99 → 2.83 V (salida débil, alimentada por diodo).
- Osciloscopio: **ráfagas de ~6–10 µs cada ~46–50 µs (~21 kHz)** en SW1/SW2, nunca conmutación continua a 350 kHz.
- Cámara térmica a 7.2 V / 100 mA: el LM5175 es el único punto caliente de la placa (máx. ~34.5 °C con ambiente ~26 °C). Ver [img/placa1_termica_7V2_100mA.jpg](img/placa1_termica_7V2_100mA.jpg).

## Mecanismo

Al habilitarse, el chip enciende Q2 (LDRV1) y se forma el camino VCC → D3 → BOOT2 → (fuga interna del LM5175) → SW2 → L1 → SW1 → Q2 → GND. Esa corriente supera el límite del regulador de VCC (≥ 65 mA), VCC cae al UVLO (3.27 V), el chip se reinicia y reintenta cada ~50 µs. Los ~3 V de salida son VCC colándose por BIAS hacia VOUT, no regulación.

## Conclusión

LM5175 con la etapa high-side 2 (BOOT2/SW2) dañada internamente (defecto de fábrica, ESD o manejo). Nada en el diseño excede el máximo absoluto de BOOT–SW (8.5 V). La Placa 2, del mismo pedido y diseño, pasó las mismas mediciones, lo que descarta el diseño y los archivos de ensamblaje.

## Pendiente

- Retirar el LM5175 y medir la **huella vacía pad 20 ↔ pad 18** (debe dar abierto) y el **chip suelto pin 20 ↔ 18** (debe mostrar la fuga).
- Montar un chip nuevo con precauciones ESD y repetir el protocolo de [02](02_protocolo_bringup.md).
