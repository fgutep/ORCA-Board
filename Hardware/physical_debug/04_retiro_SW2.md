# 04 — Retiro de SW2 (BOOT) y verificación

**Estado:** ✅ SW2 desoldado el 10 de noviembre de 2026. El firmware corre.

## Por qué

SW2 deja IO0 (U2 pin 27) en corto a GND (ver [02_errata_SW1_SW2.md](02_errata_SW1_SW2.md)), así que el chip siempre arranca en modo descarga. Sirvió para flashear la placa por primera vez, pero mientras siga montado **ningún firmware corre**.

## Procedimiento

1. Desconectar el USB.
2. Desoldar SW2, **o** cortar dos patas diagonalmente opuestas si se quiere conservar el botón.
3. Sin alimentación: verificar que **no hay continuidad** entre C4 (lado IO0) y GND.
4. Conectar el USB.

## Checklist de verificación

| # | Prueba | Esperado | Resultado |
|---|---|---|---|
| 1 | IO0 (C4) con alimentación | ~3.3 V (pull-up interno) | |
| 2 | `lsusb` | `303a:1001 Espressif USB JTAG/serial debug unit` | |
| 3 | Log del ROM tras reset | `boot:0x8 (SPI_FAST_FLASH_BOOT)` en lugar de `boot:0x20 (DOWNLOAD…)` | |
| 4 | Monitor serie (Arduino IDE o `pio device monitor`, 115200) | Reporte de `perif_test` cada 5 s, encabezado `===== ORCA perif_test #N \| ESP32-S3 rev v0.2 \| MAC 80:45:6b:52:f6:2c …` | |
| 5 | Flasheo sin botones (`pio run -t upload` o Arduino IDE) | esptool entra solo a modo descarga, `Hash of data verified`, la app vuelve a correr | |

## Resultado esperado de `perif_test` (antes del retrabajo INA226/PCA9685)

| Bloque | Línea | Esperado |
|---|---|---|
| I2C placa | `dispositivos:` | `0x40 0x4A 0x70` (0x4B en vez de 0x4A según el BNO085) |
| | INA226 | `[FAIL] … ID = 0x1408 …: colision INA226/PCA9685` → confirma el conflicto. Tras el retrabajo: `[PASS] INA226 en 0x40 (ID 0x5449) \| Vbus = …` |
| | PCA9685 | `[INFO] Nada en 0x41`. Tras el retrabajo: `[PASS] PCA9685 en 0x41 \| MODE1 = 0x11` |
| | All Call / BNO085 | `[PASS]` |
| Mux I2C | TCA9548A | `[PASS] … control escrito 0x01, leido 0x01` |
| | CH0–CH3 | `(ninguno)` sin módulos conectados |
| Isens | VIOUT | `[PASS] VIOUT = ~330 mV -> I = ~0.00 A` sin carga |
| CAN | eco | `[PASS] eco recibido \| estado RUNNING, TEC 0 …`. Si TEC sube y no hay eco: revisar U7, IO6/IO7 o la terminación R15 |

El valor `0x1408` sale del AND en el bus entre el ID del INA226 (`0x5449`) y el registro PRE_SCALE del PCA9685 (`0x1E`, repetido porque el auto-incremento está apagado al encender). Si sale otro valor distinto de `0x5449`, igual indica colisión o falla.

Vsens solo mide si hay batería en J19. Isens no necesita la batería de motores para leer el cero.

Si el paso 3 sigue mostrando `DOWNLOAD`, IO0 todavía está en bajo: revisar puentes de estaño en los pads de SW2.

---

## Resultado (10 de noviembre de 2026)

### Arranque

1. **Primer encendido tras desoldar SW2: arrancó en modo descarga** (`boot:0x21 (DOWNLOAD(USB/UART0))`). Lectura con esptool (solo lectura):

   | Registro | Valor | Interpretación |
   |---|---|---|
   | `GPIO_STRAP_REG` (0x60004038) | `0x21` | bit 3 = 0 → IO0 estaba en bajo cuando se latchearon los straps |
   | `GPIO_IN_REG` (0x6000403C) | `0x3c064801` | bit 0 = 1 → IO0 en alto en ese momento |
   | `IO_MUX_GPIO0_REG` (0x60009004) | `0xb00` | pull-up interno activo, entrada habilitada |
   | `RTC_CNTL_OPTION1_REG` (0x6000812C) | `0x0` | sin "force download" por USB |

   Los resets por USB-Serial/JTAG (RTS, `--after hard-reset`) **no vuelven a latchear los straps**: siguió en modo descarga. Un reset por watchdog sí los relee y la app arrancó:

   ```bash
   esptool --port <puerto> --before no-reset --after watchdog-reset chip-id
   ```

2. **Reconexión del USB: arrancó normal** y `perif_test` corrió desde `#0`. El arranque en modo descarga anterior no se repitió.

   Se sospechó que C4 (100 nF en IO0, cargando por el pull-up interno de ~45 kΩ, τ ≈ 4.5 ms) era más lento que EN (R1 10 kΩ × C3 100 nF, τ = 1 ms). **No se confirmó**: con un encendido limpio arranca bien. Si vuelve a pasar con una reconexión normal, la primera prueba es retirar C4.

### `perif_test`

| Bloque | Resultado |
|---|---|
| I2C placa: escaneo | `0x40 0x70` |
| INA226 | ❌ `ID = 0x1408` (esperado `0x5449`) → **colisión con el PCA9685 confirmada**. Vbus crudo `0x0000`. ✅ **Resuelto** tras el retrabajo de [06](06_retrabajo_I2C_INA226_PCA9685.md): ID `0x5449` |
| PCA9685 en 0x41 | Nada (sin retrabajo todavía). ✅ **Tras el retrabajo:** presente, MODE1 `0x11` |
| PCA9685 All Call 0x70 | ✅ |
| BNO085 0x4A/0x4B | ⏸️ No responde: **el módulo no está montado todavía** |
| TCA9548A | ✅ en 0x70, control `0x01` escrito y leído |
| Canales CH0–CH3 | `(ninguno)`, sin módulos conectados |
| Isens | ✅ VIOUT = 316–319 mV → −0.04 a −0.05 A (offset del ADC) |
| CAN | ✅ eco recibido, `RUNNING`, TEC 0, REC 0, 0 errores de bus |

En el PC, tras el reset por watchdog el puerto volvió como `/dev/ttyACM1`. Usar la ruta estable:

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:52:F6:2C-if00
```
