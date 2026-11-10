# 05 — Revisión de esquemático: CAN, multiplexor I2C, Isens y Vsens

**Fecha:** 10 de noviembre de 2026
**Método:** netlist exportado con `kicad-cli` desde `ORCA-BOARD.kicad_sch` (sin cambios respecto al commit `98bd294`), comparado contra los datasheets de `Datasheets/`. Paridad esquemático ↔ PCB verificada con `kicad-cli pcb drc --schematic-parity`: sin diferencias en componentes reales, 0 ítems sin conectar.

> Revisión de papel. Las pruebas funcionales quedan para después de retirar SW2 (ver "Pruebas con firmware" al final).

## Resumen

| Bloque | Pieza | Veredicto |
|---|---|---|
| CAN | U7 SN65HVD230 | ✅ Debería funcionar |
| Multiplexor I2C externo | U4 TCA9548A | ⚠️ **SDA/SCL cruzados en los 4 conectores** + sin pull-ups en los canales |
| Isens (corriente de motores) | U12 ACS725LLCTR-10AU | ✅ Debería funcionar |
| Vsens (voltaje de batería) | U11 INA226 | ❌ **Misma dirección I2C (0x40) que el PCA9685** → ✅ **Resuelto** el 10/11/2026: PCA9685 movido a 0x41 ([06](06_retrabajo_I2C_INA226_PCA9685.md)) |

---

## CAN — U7 SN65HVD230 ✅

| Pin | Red | Revisión |
|---|---|---|
| 1 D | `/CAN_TX` → IO6 (U2.6) | ✅ |
| 4 R | `/CAN_RX` → IO7 (U2.7) | ✅ |
| 3 VCC | +3V3 | ✅ (pieza de 3.3 V) |
| 2 GND | GND | ✅ |
| 7 CANH / 6 CANL | J2.1 / J2.2, R15 = 120 Ω entre ambos | ✅ terminación de bus |
| 8 Rs | R16 = 1 kΩ a GND | ✅ modo alta velocidad (datasheet: Rs a GND; 10–100 kΩ = control de pendiente; alto = standby) |
| 5 Vref | sin conectar | ✅ opcional |

El TWAI del ESP32-S3 se puede asignar a cualquier GPIO, así que IO6/IO7 sirven.

**Observaciones (no impiden que funcione):**
- **J2 no tiene pin de GND** (JST ZH de 2 pines, solo CANH/CANL). Los nodos del bus necesitan una referencia de tierra común por otro camino (por ejemplo, el cable de potencia), o el modo común se sale del rango del transceptor (−2 a 7 V).
- **R15 de 120 Ω fija:** esta placa tiene que ir en un extremo del bus.
- **U7 no tiene condensador de desacople propio** en el esquemático (el 3V3 más cercano es C2 a ~8.5 mm).

---

## Multiplexor I2C externo — U4 TCA9548A ⚠️

Lado maestro correcto:

| Pin | Red | Revisión |
|---|---|---|
| 22 SCL | `/EXT_I2C_SCL` → IO38, R7 10 kΩ a +3V3 | ✅ |
| 23 SDA | `/EXT_I2C_SDA` → IO39, R8 10 kΩ a +3V3 | ✅ |
| 3 RESET | `/EXT_I2C_RST` → IO40 | ✅ (ver observación) |
| 1, 2, 21 A0–A2 | GND | ✅ dirección **0x70** |
| 24 VCC / 12 GND | +3V3 / GND | ✅ |

### ❌ SDA y SCL cruzados en todos los canales

Según el datasheet (Tabla 4-1), **SDn = dato** y **SCn = reloj**. Las etiquetas de red están al revés:

| Canal | Pin U4 | Función real | Etiqueta de red | Pin del conector |
|---|---|---|---|---|
| 0 | 4 SD0 | **dato** | `I2C_SCL_CH0` | J14.4 |
| 0 | 5 SC0 | **reloj** | `I2C_SDA_CH0` | J14.3 |
| 1 | 6 SD1 / 7 SC1 | dato / reloj | `SCL_CH1` / `SDA_CH1` | J15.4 / J15.3 |
| 2 | 8 SD2 / 9 SC2 | dato / reloj | `SCL_CH2` / `SDA_CH2` | J16.4 / J16.3 |
| 3 | 10 SD3 / 11 SC3 | dato / reloj | `SCL_CH3` / `SDA_CH3` | J17.4 / J17.3 |

**Pinout real de J14–J17:**

| Pin | J14, J15 | J16, J17 |
|---|---|---|
| 1 | GND | GND |
| 2 | **5 V** | **3.3 V** |
| 3 | **SCL** (aunque la red diga SDA) | **SCL** |
| 4 | **SDA** (aunque la red diga SCL) | **SDA** |

**En la placa actual:** se soluciona en el cable, intercambiando los hilos 3 y 4 respecto a lo que indica el esquemático. **Rev B:** corregir las etiquetas (o el ruteo, si se quiere SDA en el pin 3).

### ⚠️ Sin pull-ups en los canales

El datasheet pide pull-up en cada SDn/SCn. En la placa no hay ninguno. El TCA9548A es un interruptor de transistores de paso: con VCC = 3.3 V, el pull-up del lado maestro (R7/R8) solo lleva el canal a ~VCC − Vth (~2 V), por debajo del VIH de un dispositivo de 3.3 V.

- Funciona si **cada módulo conectado trae sus propias pull-ups** (la mayoría de los breakouts de Adafruit/SparkFun las traen). En J14/J15 las pull-ups del módulo pueden ir a 5 V: el TCA9548A tolera 5 V en los canales y así traduce niveles.
- Con un sensor "pelado" sin pull-ups, ese canal no va a funcionar.
- **Rev B:** pull-ups de 4.7–10 kΩ por canal, al voltaje de su conector.

**Observaciones:**
- RESET (activo en bajo) no tiene pull-up. Mientras IO40 esté sin configurar, el mux queda en un estado indefinido. El firmware debe poner IO40 en alto al arrancar. Rev B: pull-up de 10 kΩ.
- Los canales 4–7 no salen a ningún conector (CH4 tiene etiquetas pero no llega a nada).
- U4 no tiene condensador de desacople propio (el 3V3 más cercano está a ~22 mm).

---

## Isens — U12 ACS725LLCTR-10AU ✅

Mide la corriente de los motores: entra por J20 (XT60, red `VM`) y sale como `VMout` hacia los DRV8874 (U5, U6) y sus condensadores de bulk.

| Pin | Red | Revisión |
|---|---|---|
| 1, 2 IP+ | `VM` (J20.2) | ✅ |
| 3, 4 IP− | `VMout` → U5.11, U6.11 | ✅ sentido correcto para la versión unidireccional |
| 8 VCC | +3V3, C16 0.1 µF (~4.7 mm) | ✅ el ACS725 es de 3.0–3.6 V |
| 6 FILTER | C17 = 1 nF a GND | ✅ |
| 7 VIOUT | `/SensCurrent_IN` → IO9 (U2.17) | ✅ IO9 = ADC1_CH8, funciona con Wi-Fi activo |
| 5 GND | GND | ✅ |

**Conversión (datasheet, VCC = 3.3 V):**

| Corriente | VIOUT |
|---|---|
| 0 A | 0.1 × VCC = **0.33 V** |
| 10 A | 0.33 + 10 × 0.264 = **2.97 V** |

I = (V_IO9 − V_cero) / 0.264 V/A. La salida es ratiométrica: conviene medir V_cero al arrancar, con los motores quietos. Usar atenuación de 11/12 dB en el ADC. 2.97 V está cerca del límite superior del ADC del S3, donde es menos lineal.

**Observaciones:**
- Solo ve la corriente de los motores DC. Los servos se alimentan de `5V_SERVOS` (buck U8 desde `VBUS`) y no pasan por U12.
- Es unidireccional: la corriente de frenado regenerativo (inversa) no se mide, la salida se satura en ~0 A.

---

## Vsens — U11 INA226 ❌ → ✅ resuelto con retrabajo

| Pin | Red | Revisión |
|---|---|---|
| 8 VBUS | `VBUS` (J19.2, batería 2S–6S) | ✅ 25.2 V máx. de 6S < 36 V del INA226 |
| 10 IN+ / 9 IN− | **ambos en `VBUS`** | ⚠️ sin shunt: solo mide voltaje (corriente y potencia siempre 0). Coherente con la hoja `Sensor_voltaje`, confirmar que era la intención |
| 6 VS | +3V3, C14 0.1 µF | ✅ (pero C14 está a ~25 mm) |
| 4 SDA / 5 SCL | `/OB_I2C_SDA` (IO18) / `/OB_I2C_SCL` (IO17), R3/R2 10 kΩ | ✅ |
| 2 A0 / 1 A1 | GND / GND | ❌ ver abajo |
| 3 ALERT | sin conectar | ✅ |

### ❌ Conflicto de dirección con el PCA9685

| Dispositivo en `OB_I2C` | Pines de dirección | Dirección |
|---|---|---|
| U11 INA226 | A1 = GND, A0 = GND | **0x40** (datasheet, Tabla 6-2) |
| U9 PCA9685 | A0–A5 = GND | **0x40** |
| U1 BNO085 (Adafruit) | — | 0x4A |
| U9 PCA9685 LED All Call | habilitada al encender | 0x70 |

INA226 y PCA9685 responden los dos en 0x40: las lecturas chocan en el bus y las escrituras al INA226 también llegan a los registros del PCA9685 (y al revés). **Así no funcionan ni Vsens ni los servos.**

> ✅ **Resuelto en la placa (10/11/2026):** A0 del PCA9685 (U9 pin 1) levantado y puenteado a VDD (pin 28) → PCA9685 en **0x41**. `perif_test`: escaneo `0x40 0x41 0x70`, INA226 ID `0x5449`, PCA9685 MODE1 `0x11`. Ver [06](06_retrabajo_I2C_INA226_PCA9685.md). En Rev B sigue pendiente corregirlo en el esquemático.

**Retrabajo en la placa actual (cambiar una de las dos direcciones):**

| Opción | Acción | Nueva dirección | Dificultad |
|---|---|---|---|
| **A (recomendada)** | U9 PCA9685 (TSSOP-28, 0.65 mm): levantar el pin 1 (A0) y puentearlo al pin 28 (VDD, en el mismo extremo del encapsulado, del lado opuesto) | PCA9685 → **0x41** | Media |
| B | U11 INA226 (TSSOP-10, 0.5 mm): levantar el pin 2 (A0) y puentearlo al pin 4 (SDA) o al pin 5 (SCL), del mismo lado | INA226 → **0x42** / **0x43** | Difícil (paso fino) |

**Rev B:** poner A0 del INA226 a VS (0x41) o los pines de dirección del PCA9685 a otra combinación.

---

## Pruebas con firmware (después de retirar SW2)

| Bloque | Prueba | Esperado |
|---|---|---|
| OB_I2C (IO17/IO18) | Escaneo I2C | 0x4A (BNO085), 0x70 (PCA9685 All Call) y 0x40 compartida. Tras el retrabajo: 0x40 y 0x41 por separado |
| EXT_I2C (IO38/IO39) | IO40 en alto, escaneo | 0x70 (TCA9548A). Escribir `0x01` y leer de vuelta `0x01` |
| Canales del mux | Módulo con pull-ups en J14–J17 (con el cable con 3↔4 intercambiados) | Aparece al seleccionar su canal |
| Isens | ADC en IO9 sin carga | ~0.33 V |
| Vsens | Registro de bus voltage del INA226 (1.25 mV/LSB) | Voltaje de la batería en J19 |
| CAN | TWAI en modo `NO_ACK` / self-test con IO6/IO7 | Transmite sin errores de bus; con un segundo nodo, intercambio de tramas |
