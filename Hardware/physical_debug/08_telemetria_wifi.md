# 08 — Telemetría por Wi-Fi para las pruebas de potencia

**Estado:** ✅ usado en la prueba de VBUS del 10 de noviembre de 2026 (modo AP). Reemplazado después por `perif_test` para verificar el retrabajo del PCA9685; volver a flashearlo para las próximas pruebas de potencia.

**Lectura al terminar la prueba de potencia:** `boot_count` = 662, `brownouts` = 0. Los cortes con la fuente limitada a < 300 mA no aparecen como `BROWNOUT`: el riel cayó del todo y el chip arrancó desde cero (reset de encendido), así que no los ve el detector de brownout. Para la próxima prueba, mirar `boot_count` y los eventos `reboot` del CSV, no solo `brownouts`.

## Qué hace

Firmware [`wifi_telemetry/wifi_telemetry.ino`](wifi_telemetry/wifi_telemetry.ino):

| Dato | Fuente | Notas |
|---|---|---|
| `uptime_ms`, `boot_count`, `reset_reason`, `brownouts` | `esp_reset_reason()` + contador en NVS | El contador sobrevive a cortes de energía: al final de la prueba dice cuántos reinicios y brownouts hubo |
| `vbus_v`, `vbus_min_v`, `vbus_max_v` | INA226, registro de bus (1.25 mV/LSB) | `null` mientras `ina226_ok` sea `false` (colisión con el PCA9685 sin resolver) |
| `isens_mv`, `isens_a`, `isens_max_a` | ACS725 en IO9, 16 muestras cada 100 ms | Cero calibrado al arrancar (`isens_zero_mv`), con los motores quietos |
| `pca9685_at_0x41` | Escaneo I2C | `true` después del retrabajo de [06](06_retrabajo_I2C_INA226_PCA9685.md) |
| `chip_temp_c`, `free_heap`, `min_free_heap` | ESP32-S3 | |
| `wifi_mode`, `ip`, `rssi_dbm`, `ap_clients` | Wi-Fi | |

Todas las lecturas I2C son de solo lectura.

**Endpoints:**

| URL | Contenido |
|---|---|
| `http://192.168.4.1/` | Página que se actualiza cada segundo |
| `http://192.168.4.1/status.json` | JSON con los datos de la tabla |
| `http://192.168.4.1/reset_max` | Reinicia `isens_max_a`, `vbus_min_v` y `vbus_max_v` |

También imprime el mismo JSON por USB cada segundo.

## Wi-Fi

- **Por defecto (AP):** la placa crea la red **`ORCA-Board`**, clave **`orca1234`**, IP **192.168.4.1**. El PC se conecta a esa red.
- **Red existente (STA):** compilar con
  ```bash
  --build-property 'compiler.cpp.extra_flags=-DORCA_WIFI_SSID="MiRed" -DORCA_WIFI_PASS="clave"'
  ```
  (Usar `compiler.cpp.extra_flags`, no `build.extra_flags`: este último reemplaza los defines de modo USB de la placa.)
  Queda accesible como `orca-board.local` (mDNS) o por la IP que muestre el JSON por USB. Si no conecta en 15 s, vuelve a modo AP.

## Monitor en el PC

[`wifi_telemetry/orca_monitor.py`](wifi_telemetry/orca_monitor.py), solo biblioteca estándar de Python:

```bash
python3 Hardware/physical_debug/wifi_telemetry/orca_monitor.py                          # modo AP
python3 Hardware/physical_debug/wifi_telemetry/orca_monitor.py --host orca-board.local  # modo STA
python3 Hardware/physical_debug/wifi_telemetry/orca_monitor.py --interval 0.2 --csv prueba_6S.csv
```

- Una línea por lectura: hora, uptime, Vbus, Vbus mín., Isens, Isens máx., temperatura, RSSI, arranques y causa.
- `*** SIN RESPUESTA` cuando la placa deja de contestar y `*** EN LÍNEA de nuevo tras N s` cuando vuelve.
- `*** REINICIO #n: … causa BROWNOUT …` cuando cambia `boot_count` o baja el uptime.
- Todo queda en un CSV (`orca_power_<fecha>.csv` por defecto), con columna `event` = `offline` / `online` / `reboot`.

Probado contra un servidor simulado (caída + reinicio con `BROWNOUT`): detecta ambos eventos.

## Flasheo

```bash
FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled"
arduino-cli compile -b "$FQBN" Hardware/physical_debug/wifi_telemetry
arduino-cli upload  -b "$FQBN" -p /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:52:F6:2C-if00 Hardware/physical_debug/wifi_telemetry
```

Reemplaza a `perif_test`. Para volver a la prueba de periféricos, flashear `perif_test` de nuevo.

**No hace falta puentear IO0 para flashear:** esptool entra solo a modo descarga por USB-Serial/JTAG. Si IO0 está puenteado a GND en el reset final, el chip vuelve a quedar en `waiting for download` (`GPIO_STRAP_REG` con bit 3 = 0). En ese caso, ya sin el puente:

```bash
esptool --port <puerto> --before no-reset --after watchdog-reset chip-id
```

Nota: después de ese reset, `reset_reason` aparece como `USB`.

**Abrir el puerto serie reinicia la placa** (Linux mueve DTR/RTS al abrir el puerto del USB-Serial/JTAG): aparece un reinicio con causa `USB`. Durante las pruebas de potencia no abrir el monitor serie.
