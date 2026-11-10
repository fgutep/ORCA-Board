# 02 — Errata: pulsadores SW1 (RESET) y SW2 (BOOT)

**Fecha:** 10 de noviembre de 2026

| # | Pieza | Red afectada | Efecto | Estado en la placa |
|---|---|---|---|---|
| E1 | SW1 (RESET) | EN (U2 pin 3) | EN en corto a GND → ESP32 en reset permanente, no enumera por USB | ✅ **SW1 desoldado** |
| E2 | SW2 (BOOT) | IO0 (U2 pin 27) | IO0 en corto a GND → siempre arranca en modo descarga, el firmware nunca corre | ⏳ **Se mantiene montado a propósito** mientras se flashea; se retira después ([04_retiro_SW2.md](04_retiro_SW2.md)) |

## Causa común: footprint con los pares internos cruzados

El pulsador de 4 patas tiene las patas unidas internamente **por pares**; al presionar se conecta un par con el otro. Se confirmó en la pieza desoldada que las patas están unidas internamente.

El footprint custom `Switch_4_pins:SW_SPST_B3S-1000_1_4_labels` numera los pads de forma que **cada par interno queda con una pata en GND y otra en la señal**:

```
 Footprint stock KiCad (B3S-1000)     Footprint custom usado en Rev A
                                      (esquemático: 1,2 = GND · 3,4 = EN / IO0)

  1 ●━━━━━━━━━━━━━━━● 1                1 ● GND ━━━━━━━━━━━━ EN ● 4   ← par interno
                                                    [ ]
  2 ●━━━━━━━━━━━━━━━● 2                2 ● GND ━━━━━━━━━━━━ EN ● 3   ← par interno

  ━━ = unidos internamente en el pulsador
```

En el stock los pads con el mismo número están a lo ancho del cuerpo (mismo par interno). En el custom los pads 1–2 quedan a un lado y 3–4 al otro, así que la señal queda en corto a GND, presionado o no.

Coordenadas en `Hardware/Custom_Footprints/Switch_4_pins.pretty/SW_SPST_B3S-1000_1_4_labels.kicad_mod`:

| Pad | Posición (mm) | SW1 | SW2 |
|---|---|---|---|
| 1 | (−3.975, −2.25) | GND | GND |
| 2 | (−3.975, +2.25) | GND | GND |
| 3 | (+3.975, +2.25) | EN | IO0 |
| 4 | (+3.975, −2.25) | EN | IO0 |

El resto de ambos circuitos es correcto:

| Red | Componentes |
|---|---|
| EN (`Net-(SW1-A)`) | U2.3, R1 = 10 kΩ a +3V3, C3 = 0.1 µF a GND, SW1.3, SW1.4 |
| IO0 (`Net-(SW2-A)`) | U2.27, C4 = 0.1 µF a GND, SW2.3, SW2.4 (pull-up interno del ESP32) |

## Cómo verificarlo

Sin alimentación, continuidad a GND:
- EN: en C3 o en el pin 3 de U2 → pita si SW1 está montado.
- IO0: en C4 o en el pin 27 de U2 → pita si SW2 está montado.

Con alimentación: EN e IO0 miden ~0 V.

## Retrabajo

**Opción usada (SW1):** desoldar el pulsador. R1 sube EN a 3.3 V.

**Alternativa que conserva el botón:** cortar o levantar **dos patas diagonalmente opuestas** (cualquiera de las dos diagonales). Cada par interno queda tocando una sola red y el pulsador funciona como debe: abierto en reposo, señal a GND al presionar.

**Sin botones en la placa:**
- Reset: esptool reinicia el chip por USB-Serial/JTAG; a mano, puentear un momento C3 (lado EN) a GND, o desconectar y reconectar el USB.
- Modo descarga: esptool lo activa solo por USB-Serial/JTAG; a mano, mantener C4 (lado IO0) a GND mientras se hace el reset.

## Corrección para Rev B

1. Usar el footprint stock `Button_Switch_SMD:SW_SPST_B3S-1000` con un símbolo de 2 pines, **o** en el footprint custom intercambiar las posiciones de los pads 2 y 4 para que 1–2 queden en el mismo par interno:

   | Pad | Posición actual | Posición corregida |
   |---|---|---|
   | 2 | (−3.975, +2.25) | (+3.975, −2.25) |
   | 4 | (+3.975, −2.25) | (−3.975, +2.25) |

2. **Verificar la pieza real:** el campo Datasheet de SW1/SW2 apunta a APEM MULTIMEC 5E, pero el footprint es de un Omron B3S-1000. Confirmar con el datasheet del pulsador que se va a comprar qué patas están unidas internamente.
3. Considerar test pads en EN e IO0 para poder hacer reset y entrar a modo descarga a mano si algún firmware deja el USB inutilizable.
