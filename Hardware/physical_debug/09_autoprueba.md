# 09 — Autoprueba completa por Wi-Fi y USB

**Estado:** ✅ flasheado el 10 de noviembre de 2026. **Imagen actual en la flash.**

Firmware [`orca_selftest/orca_selftest.ino`](orca_selftest/orca_selftest.ino): une `perif_test` y `wifi_telemetry`. Cada 5 s corre todas las pruebas y publica el mismo informe por Wi-Fi y por USB.

## Pruebas

| Bloque | Prueba | Cuenta en el resumen |
|---|---|---|
| I2C placa | Escaneo (esperado `0x40 0x41 0x70`) | Info |
| INA226 (0x40) | ID `0x5449` + Vbus | ✅ |
| INA226 | Escritura/lectura: reescribe el valor actual de config y lo relee | ✅ |
| PCA9685 (0x41) | Lectura de MODE1 (`0x11` al encender) | ✅ |
| PCA9685 | Escritura/lectura: `0x5A` en LED0_ON_L, relee y restaura. En SLEEP no sale PWM | ✅ |
| PCA9685 All Call (0x70) | Presencia | ✅ |
| BNO085 | 0x4A/0x4B | `N/A` mientras `BNO085_MOUNTED = false` |
| TCA9548A (0x70, bus externo) | Escribe `0x01` en control y lo relee | ✅ |
| Canales CH0–CH3 (J14–J17) | Escaneo de cada canal | Info |
| Isens (ACS725, IO9) | VIOUT en 250–450 mV sin carga | ✅ |
| CAN (IO6/IO7) | Trama en autorrecepción a través del SN65HVD230 | ✅ |

Cuando se monte el BNO085, poner `BNO085_MOUNTED = true` y volver a flashear.

## Cómo probar

### 1. Por Wi-Fi

1. Conectar el PC a la red **`ORCA-Board`**, clave **`orca1234`** (se pierde internet mientras tanto).
2. Abrir **http://192.168.4.1/**: informe que se actualiza cada 2 s, con `[PASS]` en verde y `[FAIL]` en rojo.
3. Opcional:
   - **http://192.168.4.1/status.json**: telemetría + bloque `selftest` con cada resultado (`true`/`false`), `pass` y `fail`.
   - **http://192.168.4.1/report.txt**: el informe en texto.
   - `python3 Hardware/physical_debug/wifi_telemetry/orca_monitor.py`: también funciona con este firmware.
4. Volver a la red normal.

### 2. Por USB

Monitor serie a 115200 en `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:52:F6:2C-if00` (Arduino IDE, `pio device monitor`, etc.). Abrir el puerto reinicia la placa (aparece `causa USB`); el informe sale a los ~5 s.

## Resultado al flashear (por USB)

```
===== ORCA selftest #2 | ESP32-S3 rev v0.2 | MAC 80:45:6b:52:f6:2c | uptime 10 s =====
[Sistema] arranque #664, causa USB, brownouts 0, 32.4 C, heap libre 240804 B
[Wi-Fi] AP ORCA-Board, IP 192.168.4.1, clientes 0
[I2C placa] IO18 SDA / IO17 SCL
  dispositivos: 0x40 0x41 0x70 (esperado 0x40 0x41 0x70)
  [PASS] INA226 en 0x40: ID 0x5449 (esperado 0x5449) | Vbus = 0.000 V
  [PASS] INA226 escritura/lectura de config: 0x4127
  [PASS] PCA9685 en 0x41: MODE1 = 0x11 (0x11 al encender = SLEEP + ALLCALL)
  [PASS] PCA9685 escritura/lectura LED0_ON_L: escrito 0x5A, leido 0x5A
  [PASS] PCA9685 LED All Call en 0x70
  [N/A ] BNO085 no montado (nada responde en 0x4A/0x4B)
[Mux I2C] IO39 SDA / IO38 SCL / IO40 RESET
  [PASS] TCA9548A en 0x70: control escrito 0x01, leido 0x01
  [INFO] CH0 J14 (5V) : (ninguno)
  [INFO] CH1 J15 (5V) : (ninguno)
  [INFO] CH2 J16 (3V3): (ninguno)
  [INFO] CH3 J17 (3V3): (ninguno)
[Isens] ACS725 en IO9 (0 A = 0.1 x VCC, 264 mV/A)
  [PASS] VIOUT = 318 mV -> I = -0.001 A (cero calibrado 319 mV, max 0.039 A)
[CAN] SN65HVD230, TX IO6 / RX IO7, 500 kbit/s, NO_ACK + autorrecepcion
  [PASS] eco recibido | estado RUNNING, TEC 0, REC 0, errores de bus 0
===== RESUMEN: 8 PASS, 0 FAIL =====
```

Red `ORCA-Board` visible desde el PC (WPA2, canal 1).

## Verificación completa (10 de noviembre de 2026)

| Vía | Resultado |
|---|---|
| Wi-Fi (página en 192.168.4.1) | ✅ Probado por el equipo |
| USB (monitor serie) | ✅ **8 PASS, 0 FAIL** |

En la lectura por USB la placa estaba alimentada **a la vez por VBUS (fuente a 8 V) y por USB**: el INA226 midió **Vbus = 8.002 V**, primera lectura real de Vsens tras el retrabajo del PCA9685. Confirma también que VBUS y USB juntos funcionan sin problema (ver P9 en [07](07_revision_potencia.md)).

```
[Sistema] arranque #666, causa USB, brownouts 0, 35.4 C, heap libre 240804 B
  dispositivos: 0x40 0x41 0x70 (esperado 0x40 0x41 0x70)
  [PASS] INA226 en 0x40: ID 0x5449 (esperado 0x5449) | Vbus = 8.002 V
  ...
  [PASS] VIOUT = 321 mV -> I = 0.013 A (cero calibrado 318 mV, max 0.046 A)
  [PASS] eco recibido | estado RUNNING, TEC 0, REC 0, errores de bus 0
===== RESUMEN: 8 PASS, 0 FAIL =====
```

## Flasheo

```bash
FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled"
arduino-cli compile -b "$FQBN" Hardware/physical_debug/orca_selftest
arduino-cli upload  -b "$FQBN" -p /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:52:F6:2C-if00 Hardware/physical_debug/orca_selftest
```
