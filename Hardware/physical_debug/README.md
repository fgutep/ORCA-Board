# Physical debug — ORCA-BOARD Control Rev A

Registro del bring-up físico de la PCB de control (ESP32-S3-WROOM-1) Rev A fabricada.

**Inicio:** 10 de noviembre de 2026

## Estado actual

| Ítem | Estado |
|---|---|
| Alimentación +3V3 (pin 2 de U2) | ✅ 3.3 V por USB |
| EN (pin 3 de U2) | ✅ Alto tras desoldar SW1 |
| Enumeración USB | ✅ `303a:1001` en `/dev/ttyACM0` |
| Comunicación con esptool | ✅ ESP32-S3 v0.2, 4 MB flash, sin PSRAM |
| Bootloader + particiones + app de prueba | ✅ Flasheados y verificados (Arduino IDE/CLI y PlatformIO) |
| IO0 (pin 27 de U2) | ✅ SW2 desoldado |
| Firmware corriendo | ✅ Arranca desde la flash al conectar el USB (probado con `perif_test` y `wifi_telemetry`) |
| CAN (U7) | ✅ Probado: eco por el transceptor sin errores de bus |
| Mux I2C (U4) | ✅ Probado en 0x70. ⚠️ SDA/SCL cruzados en J14–J17 (se corrige en el cable) y sin pull-ups por canal |
| Isens (U12 ACS725) | ✅ Probado: ~318 mV sin carga |
| Vsens (U11 INA226) | ✅ **Resuelto:** colisión en 0x40 con el PCA9685 confirmada en la placa y corregida moviendo el PCA9685 a 0x41 ([06](06_retrabajo_I2C_INA226_PCA9685.md)). INA226 ID `0x5449` |
| PCA9685 (U9) | ✅ En **0x41** tras el retrabajo, MODE1 `0x11` |
| Potencia VBUS | ✅ Estable de 8 a 22 V; a 8 V hace falta ≥ 300 mA de límite en la fuente ([07](07_revision_potencia.md)) |
| BNO085 (U1) | ✅ Montado; lo usa el firmware micro-ROS (`/imu`) |
| micro-ROS | ✅ Nodo `/orca_board` en ROS 2 Jazzy, probado por USB ([firmware](../../firmware/orca_micro_ros/README.md)) |
| Integración SWARM | ✅ La placa, integrada a un robot SWARM, respondió a señales de movimiento enviadas por Wi-Fi desde ROS 2 en una Raspberry Pi 5 esclavizada a la placa de lógica ([video](https://youtu.be/ohQkxq8agtg)) |
| Telemetría Wi-Fi | ✅ Probada durante la prueba de potencia |
| Autoprueba completa | ✅ `orca_selftest` en la flash: 8 PASS, 0 FAIL por Wi-Fi y por USB ([09](09_autoprueba.md)) |
| Motores (U5/U6 DRV8874) | ✅ Probados a 12 V con N20 y Faulhaber, libres y bloqueados, hasta 1.37 A sostenidos y 4.25 A de pico con los dos canales, con VM desde la Placa 2 de potencia ([11](11_ensayo_motores_N20_Faulhaber.md)). 🟠 Faulhaber consume más a duty parcial (probable rizado de PWM). Temperatura de pistas sin medir. ⚠️ nFAULT e IPROPI no llegan al ESP32 |
| Vsens con batería/fuente | ✅ INA226 midió 8.002 V con la fuente a 8 V (VBUS + USB a la vez) |
| 5VUSB con alimentación por VBUS | ✅ ~2 V sin carga = solo fuga de D2; 0 mV con 10 kΩ. VBUS + USB a la vez es seguro ([07](07_revision_potencia.md), P9) |

**Ensamblaje de la placa fabricada:** D1 sin poblar, R6 = 0 Ω.

## Documentos

| Archivo | Contenido |
|---|---|
| [01_deteccion_USB.md](01_deteccion_USB.md) | Cómo se diagnosticó que el ESP32 no aparecía por USB y lectura del chip |
| [02_errata_SW1_SW2.md](02_errata_SW1_SW2.md) | Footprint de los pulsadores RESET/BOOT con pares internos cruzados |
| [03_flasheo_bootloader.md](03_flasheo_bootloader.md) | Qué se flasheó, mapa de memoria y cómo flashear con ESP-IDF, PlatformIO y Arduino IDE |
| [04_retiro_SW2.md](04_retiro_SW2.md) | Procedimiento y checklist de verificación al retirar SW2 |
| [05_revision_CAN_I2C_sensores.md](05_revision_CAN_I2C_sensores.md) | Revisión de esquemático de CAN, mux I2C, Isens y Vsens contra datasheets |
| [06_retrabajo_I2C_INA226_PCA9685.md](06_retrabajo_I2C_INA226_PCA9685.md) | Procedimiento para mover el PCA9685 a 0x41 |
| [07_revision_potencia.md](07_revision_potencia.md) | Revisión de VBUS, buck de 5 V, 3V3 y VM (esquemático + PCB) y plan de prueba de potencia |
| [08_telemetria_wifi.md](08_telemetria_wifi.md) | Firmware de telemetría por Wi-Fi y monitor en el PC para las pruebas de potencia |
| [09_autoprueba.md](09_autoprueba.md) | Autoprueba completa (periféricos + telemetría) por Wi-Fi y USB |
| [10_banco_motores.md](10_banco_motores.md) | Banco de prueba de los DRV8874: firmware, pruebas automáticas y procedimiento de carga |
| [11_ensayo_motores_N20_Faulhaber.md](11_ensayo_motores_N20_Faulhaber.md) | Primer ensayo con motores (20-nov): curvas de corriente de N20 y Faulhaber, bloqueo, dos canales. Datos en [`ensayos_motores/2026-11-20/`](ensayos_motores/2026-11-20/) |

## Firmware de prueba

- [`bringup_hello/bringup_hello.ino`](bringup_hello/bringup_hello.ino): imprime modelo, revisión, flash, PSRAM, MAC y uptime cada segundo por el USB nativo.
- [`platformio_bringup/platformio.ini`](platformio_bringup/platformio.ini): proyecto PlatformIO que compila el mismo sketch con la configuración de la placa.
- [`perif_test/perif_test.ino`](perif_test/perif_test.ino): cada 5 s prueba I2C de placa (INA226, PCA9685, BNO085), mux TCA9548A y sus 4 canales, Isens (ACS725) y CAN (autorrecepción por el transceptor). Las pruebas I2C son de solo lectura. Resultado esperado en [04_retiro_SW2.md](04_retiro_SW2.md).
- [`wifi_telemetry/`](wifi_telemetry/): firmware que publica Vbus, Isens, reinicios y brownouts por Wi-Fi, y `orca_monitor.py` para registrarlo desde el PC. Ver [08_telemetria_wifi.md](08_telemetria_wifi.md).
- [`orca_selftest/orca_selftest.ino`](orca_selftest/orca_selftest.ino): **imagen actual en la flash.** Une las dos anteriores: todas las pruebas de periféricos (con escritura/lectura verificada en INA226, PCA9685 y TCA9548A) por Wi-Fi y USB, más la telemetría. Ver [09_autoprueba.md](09_autoprueba.md).
- [`orca_motor_bench/`](orca_motor_bench/): banco de motores. Firmware PlatformIO que maneja U5/U6 con rampa, límite de corriente y deadman. Desde el panel web: control manual, STOP enclavado (HALT hasta Liberar) y secuencias de prueba (barrido, escalón, inversión, marcha continua, ambos motores) con informe y CSV. `plot_runs.py` genera curvas, series de tiempo y tablas a partir de los CSV. Ver [10_banco_motores.md](10_banco_motores.md).


Los ejemplos que usan la placa en un proyecto (brazo por gestos: BNO085 como IMU y PCA9685 con servos) están en [`examples/`](../../examples/).
