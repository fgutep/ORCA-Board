# 03 — Flasheo del bootloader y configuración para ESP-IDF, PlatformIO y Arduino IDE

**Fecha:** 10 de noviembre de 2026

## Qué significa "quemar el bootloader" en el ESP32-S3

El ESP32-S3 tiene dos etapas de arranque:

| Etapa | Dónde vive | ¿Se graba? |
|---|---|---|
| **ROM bootloader** (1ª etapa) | ROM de máscara dentro del chip | No se puede ni hace falta. Es el que habla con esptool por USB-Serial/JTAG y el que nos dio `waiting for download`. |
| **Bootloader de 2ª etapa** | Flash, offset `0x0` | Sí. Lo genera cada framework y se escribe junto con la app en cada flasheo completo. |

Por eso la placa ya era flasheable desde que enumeró por USB: no hay que grabar nada especial para que ESP-IDF, PlatformIO o Arduino IDE puedan programarla. Lo que se hizo fue dejar la flash en un estado conocido con un bootloader de 2ª etapa, la tabla de particiones y una app de prueba.

> ⚠️ **No se tocaron eFuses.** Quemar eFuses (`espefuse burn-*`) es irreversible y no es necesario para flashear. No hacerlo salvo que haya una razón concreta (secure boot, flash encryption, etc.).

## Aprovechar SW2 mientras siga montado

Con SW2 montado, IO0 está en bajo en cada reset, así que el chip **siempre** arranca en modo descarga. Mientras se flashea esto no estorba (esptool encuentra el chip esperando), pero la app nunca llega a correr. Log del ROM tras un reset por USB:

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0x20 (DOWNLOAD(USB/UART0))
Saved PC:0x40041a76
waiting for download
```

`boot:0x20 (DOWNLOAD…)` confirma que IO0 estaba en bajo al leer los pines de strapping.

## Qué se hizo

1. Borrado completo de la flash:
   ```bash
   ~/.arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool --port /dev/ttyACM0 erase-flash
   ```
2. Compilación y flasheo de [`bringup_hello`](bringup_hello/bringup_hello.ino) con **arduino-cli** (core `esp32:esp32` 3.3.11, esptool v5.3.1).
3. Compilación y flasheo del mismo sketch con **PlatformIO** (espressif32 7.1.2, framework-arduinoespressif32 4.20017, esptool v4.11.0) para validar esa ruta.
4. Re-flasheo con **arduino-cli** (core 3.3.11).
5. Flasheo de [`perif_test`](perif_test/perif_test.ino) con **arduino-cli** (core 3.3.11). Tras el reset, el ROM sigue mostrando `boot:0x20 (DOWNLOAD…)` porque SW2 sigue montado.
6. Con SW2 ya retirado: flasheo de [`wifi_telemetry`](wifi_telemetry/wifi_telemetry.ino). **Esta es la imagen actual en la flash** (ver [08_telemetria_wifi.md](08_telemetria_wifi.md)).

Todas las regiones se escribieron con `Hash of data verified`.

### Mapa de memoria (partición `default`, 4 MB)

| Offset | Contenido | Tamaño escrito |
|---|---|---|
| `0x0000` | Bootloader de 2ª etapa | 15–20 kB |
| `0x8000` | Tabla de particiones | 3 kB |
| `0xE000` | `boot_app0.bin` (selector de partición OTA) | 8 kB |
| `0x10000` | App (`bringup_hello`) | ~260–300 kB |

Tabla `default.csv`: nvs 20 kB, otadata 8 kB, app0 1.25 MB, app1 1.25 MB, spiffs 1.375 MB, coredump 64 kB.

## Configuración de la placa

Datos fijos del hardware que todo framework necesita:

| Parámetro | Valor |
|---|---|
| Chip | ESP32-S3 |
| Flash | **4 MB**, QIO, 80 MHz |
| PSRAM | **Ninguna** — activarla hace que el firmware se cuelgue al arrancar |
| Puerto | `/dev/ttyACM0` (USB nativo, USB-Serial/JTAG) |
| Consola | USB-Serial/JTAG (no hay puente USB-UART; UART0 TX/RX = pines 37/36 sin conectar) |

### Arduino IDE

**Herramientas →**

| Opción | Valor |
|---|---|
| Placa | ESP32S3 Dev Module |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | **Enabled** (si no, `Serial` no sale por el USB) |
| Upload Mode | UART0 / Hardware CDC |
| Flash Mode | QIO 80MHz |
| Flash Size | **4MB (32Mb)** |
| Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
| PSRAM | **Disabled** |
| Puerto | `/dev/ttyACM0` |

Equivalente con **arduino-cli** (verificado):

```bash
FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled"
arduino-cli compile -b "$FQBN" Hardware/physical_debug/bringup_hello
arduino-cli upload  -b "$FQBN" -p /dev/ttyACM0 Hardware/physical_debug/bringup_hello
```

### PlatformIO

Proyecto listo en [`platformio_bringup/platformio.ini`](platformio_bringup/platformio.ini) (verificado). Lo esencial para cualquier proyecto nuevo:

```ini
[env:orca_control]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_upload.flash_size = 4MB          ; la DevKitC-1 trae 8 MB por defecto
board_upload.maximum_size = 4194304
board_build.partitions = default.csv
board_build.flash_mode = qio
board_build.arduino.memory_type = qio_qspi

build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
monitor_speed = 115200
```

```bash
cd Hardware/physical_debug/platformio_bringup
pio run -t upload
pio device monitor
```

### ESP-IDF

> ESP-IDF no está instalado en este PC, así que esta ruta **no se probó**. Los parámetros son los mismos que en las otras dos.

```bash
idf.py set-target esp32s3
idf.py menuconfig
idf.py -p /dev/ttyACM0 flash monitor
```

`sdkconfig.defaults` mínimo:

```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
# CONFIG_SPIRAM is not set
```

## Precaución: no perder el acceso por USB

Con SW1 y SW2 fuera, la única forma de entrar a modo descarga es que esptool reinicie el chip por USB-Serial/JTAG. Si un firmware:

- usa IO19/IO20 como GPIO,
- cambia a USB-OTG (TinyUSB, `USBMode=default`) y se cuelga,
- o entra en deep sleep enseguida,

el puerto puede no responder. Recuperación: mantener C4 (IO0) a GND, puentear un momento C3 (EN) a GND (o reconectar el USB), soltar IO0. El chip queda en `waiting for download`.
