# 06 — Retrabajo: colisión I2C INA226 / PCA9685 en 0x40

**Estado:** ✅ resuelto el 10 de noviembre de 2026 (opción A: PCA9685 → 0x41). Ver [Resultado](#resultado).
**Problema:** U11 (INA226) y U9 (PCA9685) responden ambos en 0x40 en `OB_I2C` (ver [05_revision_CAN_I2C_sensores.md](05_revision_CAN_I2C_sensores.md)). Confirmado en la placa con `perif_test`: el ID del INA226 se lee `0x1408` en vez de `0x5449`.

## Solución: mover el PCA9685 a 0x41

Dirección del PCA9685 = `1 A5 A4 A3 A2 A1 A0`. Poniendo **A0 (pin 1) a VDD** queda en `1000001` = **0x41**. El INA226 se queda en 0x40.

### Por qué U9 y no U11

| | U9 PCA9685 (TSSOP-28) | U11 INA226 (TSSOP-10) |
|---|---|---|
| Paso | 0.65 mm | 0.5 mm |
| Pin a levantar | 1 (A0), en la esquina | 2 (A0), entre otros dos pines |
| Dónde puentear | Pin 28 (+3V3), mismo extremo, lado opuesto (5.7 mm) | Pin 4 (SDA) o 5 (SCL), mismo lado, saltando el pin 3 |
| Dirección final | PCA9685 → 0x41 | INA226 → 0x42 / 0x43 |

**No se puede cortar pista en U9:** en `F.Cu` los pads 1, 3 y 4 están unidos por una pista de GND de 1.5 mm (más ancha que los pads) y el 2 va al plano. A0–A3 forman un solo bloque de cobre a GND, así que **hay que levantar la pata**.

### ¿Sirve otro pin de dirección?

Eléctricamente sí: cualquiera de A0–A5 a VDD da una dirección libre en `OB_I2C`. Ocupadas: INA226 0x40, All Call 0x70, BNO085 0x4A/0x4B (cuando se monte). Las Sub Call (0x71/0x72/0x74) están desactivadas al encender.

| Pin a VDD | Pin # | Dirección PCA9685 | Cobre en esta placa |
|---|---|---|---|
| **A0** | **1** | **0x41** | Bajo la pista GND de 1.5 mm. **Esquina, VDD (pin 28) justo enfrente** |
| A1 | 2 | 0x42 | Bajo la pista GND de 1.5 mm, entre dos pads GND |
| A2 | 3 | 0x44 | Bajo la pista GND de 1.5 mm, entre dos pads GND |
| A3 | 4 | 0x48 | Bajo la pista GND de 1.5 mm, entre dos pads GND |
| A4 | 5 | 0x50 | El extremo redondeado de la pista de 1.5 mm se solapa con el pad 5. Además, una pista de 0.15 mm a VSS (pin 14) que pasa **bajo el encapsulado** |
| A5 | 24 | 0x60 | Unido a OE (23) y EXTCLK (25), ambos GND, por pistas de 0.15/0.3 mm en los huecos de 0.25 mm entre pads. Impráctico de cortar; el cable a VDD pasaría sobre EXTCLK, SCL y SDA |

Todas las opciones requieren levantar una pata. A0 es la más fácil.

### Ubicación (vista superior, coordenadas del PCB)

U9 en (42.28, 92.76) mm, rotado 90°, en la cara superior:

```
          28   27   26   25   24  ...  15
         +3V3 SDA  SCL       GND
          ▄    ▄    ▄    ▄    ▄         ▄        ← fila superior, y = 89.90
        ┌──────────────────────────────────┐
     ┌──┤ ●                                │
 cable  │             U9 PCA9685           │
     └──┤                                  │
        └──────────────────────────────────┘
          ▀    ▀    ▀    ▀    ▀         ▀        ← fila inferior, y = 95.62
          1    2    3    4    5   ...  14
          A0   A1   A2   A3   A4
          ↑
     levantar esta pata
```

| Pad | Red | Posición (mm) |
|---|---|---|
| 1 (A0) | GND → **levantar** | (38.05, 95.62) |
| 28 (VDD) | +3V3 | (38.05, 89.90) |
| 27 (SDA) | `/OB_I2C_SDA` | (38.70, 89.90), **a 0.65 mm del 28: no tocar** |

### Procedimiento

1. Desconectar el USB y cualquier batería.
2. Identificar el pin 1 de U9 por el punto del encapsulado: esquina con la fila de pines 1–14.
3. Poner flux en el pin 1. Calentar el pin y levantar la pata con una aguja o un bisturí fino, unos 0.3–0.5 mm, hasta que no toque el pad.
4. Poner un trozo de Kapton sobre el pad 1 (o doblar bien la pata hacia arriba) para que no vuelva a tocar el GND.
5. Soldar un cable fino (30 AWG kynar o esmaltado) a la pata levantada.
6. Llevar el cable por **fuera** del extremo izquierdo del encapsulado hasta el pin 28 y soldarlo en la **punta del pad 28** (el lado alejado del cuerpo), lejos del pin 27. Alternativa: la pista de +3V3 de 0.5 mm que sale del pin 28 hacia la izquierda, raspando la máscara.
7. Limpiar el flux.

### Verificación sin alimentación

| Medición | Esperado |
|---|---|
| Pata 1 de U9 ↔ GND | **Abierto** |
| Pata 1 de U9 ↔ +3V3 (pin 28 o C2) | **Corto** |
| Pin 27 (SDA) ↔ pin 28 | Abierto (sin puente accidental) |
| Pin 1 ↔ pin 2 | Abierto |

### Verificación con `perif_test` (no hace falta re-flashear)

| Línea | Esperado |
|---|---|
| `dispositivos:` | `0x40 0x41 0x70` |
| INA226 | `[PASS] INA226 en 0x40 (ID 0x5449) \| Vbus = …` (con batería en J19; sin batería Vbus ≈ 0 V) |
| PCA9685 | `[PASS] PCA9685 en 0x41 \| MODE1 = 0x11` |

### Firmware

Declarar el PCA9685 en **0x41**, por ejemplo `Adafruit_PWMServoDriver pwm(0x41);`. El INA226 se queda en 0x40.

## Alternativa: mover el INA226 (más difícil)

Levantar el pin 2 de U11 (A0, pad en (92.50, 54.20)) y puentearlo al pin 4 (SDA) → **0x42**, o al pin 5 (SCL) → **0x43** (datasheet INA226, Tabla 6-2). El cable pasa por encima del pin 3 (ALERT, sin conectar), con paso de 0.5 mm.

## Resultado

Retrabajo hecho (U9 pin 1 → pin 28). Verificado con `perif_test` por USB:

```
[I2C placa] IO18 SDA / IO17 SCL
  dispositivos: 0x40 0x41 0x70
  [PASS] INA226 en 0x40 (ID 0x5449) | Vbus = 0.000 V
  [PASS] PCA9685 en 0x41 | MODE1 = 0x11 (0x11 al encender)
  [PASS] PCA9685 LED All Call en 0x70
```

Vbus = 0 V porque no había batería en J19. `wifi_telemetry` también reportó `ina226_ok: true` y `pca9685_at_0x41: true`.

**Incidente durante el retrabajo:** entre las 16:29 y las 16:31 el USB falló con `error -71` (`device descriptor read/64`, `device not accepting address`, `unable to enumerate`), y la única conexión exitosa se cayó a los 10 s. Se resolvió en la placa antes de la verificación; la causa no quedó identificada. A partir de las 16:35 enumera limpio.

## Rev B

En el esquemático: A0 del INA226 a VS (0x41), o A0 del PCA9685 a VDD (0x41). Una sola de las dos.
