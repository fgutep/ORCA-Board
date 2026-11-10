# 01 — Detección del ESP32-S3 por USB

**Fecha:** 10 de noviembre de 2026
**PC:** Linux (Ubuntu 24.04), usuario en el grupo `dialout`, reglas udev de PlatformIO instaladas (cubren `303a:1001`).

## Qué se esperaba

El ESP32-S3 tiene un periférico USB-Serial/JTAG en hardware que el ROM bootloader activa al arrancar, **incluso con la flash vacía**. Con D− → IO19 (pin 13) y D+ → IO20 (pin 14) debe aparecer como:

```
ID 303a:1001 Espressif USB JTAG/serial debug unit   →   /dev/ttyACM0
```

## Síntoma

Con la placa conectada, el PC no veía nada: ningún dispositivo nuevo en `lsusb`, ningún `/dev/ttyACM*`, y **ninguna línea USB en el log del kernel**:

```bash
journalctl -k --since "-30min" | grep -i -E "usb [0-9]|descriptor|enumerate"
# (vacío)
```

El silencio total en el kernel es significativo: si el chip estuviera vivo pero con problemas en D+/D−, aparecerían errores como `device descriptor read/64, error -71` o `unable to enumerate USB device`. Sin ninguna línea, el host nunca vio el pull-up de D+ → el chip no estaba corriendo (sin alimentación o en reset) o el cable no lleva datos.

## Diagnóstico

| Paso | Medición / acción | Resultado |
|---|---|---|
| 1 | +3V3 en el pin 2 de U2 alimentando por USB | 3.3 V ✅ |
| 2 | Revisión de ruteo en `ORCA-BOARD.kicad_pcb` (USB_P, USB_N, EN, IO0) + DRC | Todo ruteado, 0 ítems sin conectar ✅ |
| 3 | EN (pin 3 de U2) | **~0 V** ❌ → chip en reset permanente |
| 4 | Revisión del circuito de EN en el netlist | R1 10 kΩ a +3V3, C3 0.1 µF a GND, SW1 entre EN y GND → correcto en esquemático |
| 5 | Revisión del footprint de SW1 contra el stock de KiCad | **Pares internos cruzados** → EN en corto a GND (ver [02_errata_SW1_SW2.md](02_errata_SW1_SW2.md)) |
| 6 | Retrabajo: SW1 desoldado | EN alto, el chip arranca |

Ruta USB verificada en el netlist:

| Señal | Conector J1 | Protección | ESP32-S3 |
|---|---|---|---|
| D− (`/USB_N`) | A7, B7 | U10 (TPD4E001) IO1 | U2.13 (IO19) |
| D+ (`/USB_P`) | A6, B6 | U10 (TPD4E001) IO2 | U2.14 (IO20) |
| CC1 / CC2 | A5 / B5 | R4 / R5 = 5.1 kΩ a GND | — |
| VBUS (`/5VUSB`) | A4, A9, B4, B9 | D2 (SS54) → `/5V` → U3 (AMS1117-3.3) | — |

## Detección tras el retrabajo

```
usb 1-2: new full-speed USB device number 5 using xhci_hcd
usb 1-2: New USB device found, idVendor=303a, idProduct=1001, bcdDevice= 1.01
cdc_acm 1-2:1.0: ttyACM0: USB ACM device
```

## Lectura del chip (solo lectura)

```bash
~/.arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool --port /dev/ttyACM0 flash-id
~/.arduino15/packages/esp32/tools/esptool_py/5.3.1/espefuse --port /dev/ttyACM0 summary
```

| Campo | Valor |
|---|---|
| Chip | ESP32-S3 (QFN56), revisión v0.2 |
| Características | Wi-Fi, BT 5 (LE), dual core + LP core, 240 MHz |
| Cristal | 40 MHz |
| Modo USB | USB-Serial/JTAG |
| MAC | `80:45:6b:52:f6:2c` |
| Flash | 4 MB, quad, fabricante 0x46, dispositivo 0x4016, 3.3 V |
| PSRAM | Ninguna → módulo **ESP32-S3-WROOM-1-N4** |
| eFuses de seguridad | Vírgenes: secure boot y flash encryption desactivados; `DIS_DOWNLOAD_MODE`, `DIS_USB_JTAG`, `DIS_USB_SERIAL_JTAG` en 0 |
