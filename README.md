<div align="center">

<img src="docs/img/banner.png" alt="ORCA-Board: Open Robotics Control Architecture" width="100%">

<br>

**Tarjeta de control abierta para robots con ROS 2, pensada para ser la base electrónica compartida de los robots de la universidad.**

<br>

![MCU](https://img.shields.io/badge/MCU-ESP32--S3-2E8B57?style=for-the-badge)
![ROS 2](https://img.shields.io/badge/ROS_2-micro--ROS-2E8B57?style=for-the-badge)
![PCB](https://img.shields.io/badge/PCB-KiCad-2E8B57?style=for-the-badge)
![Rev A](https://img.shields.io/badge/Control_Rev_A-8%2F8_autoprueba-5CB77C?style=for-the-badge)
[![Licencia MIT](https://img.shields.io/badge/Licencia-MIT-10231A?style=for-the-badge)](LICENSE)

[Resumen](#resumen) · [Características](#características) · [Estado](#estado-del-proyecto) · [Inicio rápido](#inicio-rápido) · [Estructura](#estructura-del-repositorio) · [Simulación](#simulación-de-la-etapa-de-potencia) · [Equipo](#equipo) · [Licencia](#licencia)

</div>

---

## Resumen

En la robótica académica cada proyecto suele diseñar desde cero su electrónica de potencia, actuación y sensado, y después improvisa puentes para conectarla con ROS 2. Eso repite el mismo trabajo de hardware y firmware en cada equipo. Las tarjetas comerciales de referencia, como la Acebott V3 o la Hiwonder RCB, aceptan como máximo baterías 2S o 3S, no siempre traen bus CAN y necesitan un computador intermedio para hablar con ROS.

**ORCA-Board** (*Open Robotics Control Architecture*) es una tarjeta de control abierta pensada para funcionar como nodo nativo de ROS 2 con micro-ROS. Se alimenta con baterías LiPo de **2S a 6S (7.4 a 25.2 V)** y reúne en una sola placa lo que un robot móvil o un brazo necesitan para actuar, sentir y comunicarse. El objetivo del proyecto es diseñar, fabricar y verificar esta plataforma para que distintos robots compartan la misma base electrónica.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/img/placa-control.png" alt="Placa de control ORCA-Board con ESP32-S3"><br><sub><b>Placa de control</b> · ESP32-S3</sub></td>
    <td align="center" width="33%"><img src="docs/img/placa-potencia.png" alt="Placa de potencia ORCA-Board con LM5175"><br><sub><b>Placa de potencia</b> · LM5175</sub></td>
    <td align="center" width="33%"><img src="docs/img/layout-control.png" alt="Layout PCB de la placa de control en KiCad"><br><sub><b>Layout PCB</b> · placa de control (KiCad)</sub></td>
  </tr>
</table>

## Características

El sistema se divide en **dos PCB de 4 capas** diseñadas en KiCad.

### Placa de control

| Función | Componente |
|---|---|
| Microcontrolador | ESP32-S3-WROOM-1-N4 (Wi-Fi, BLE y USB nativo; 4 MB de flash, sin PSRAM) |
| Motores DC | 2 × drivers DRV8874 |
| Servos | 16 canales PWM con PCA9685 |
| IMU | BNO085 de 9 ejes |
| Voltaje de batería | INA226 |
| Corriente de batería | ACS725 |
| Bus CAN | Transceptor SN65HVD230 |
| Sensores externos | Multiplexor I²C TCA9548A (4 canales: J14–J17) |
| Alimentación lógica | Buck TPS54331D a 5 V y regulador de 3.3 V |

### Placa de potencia

Convertidor **buck-boost LM5175** para el riel de motores: entrada de 6 a 25.2 V, salida ajustable de 6.8 a 25 V y hasta 8 A.

| Función | Componente |
|---|---|
| Controlador | U1: LM5175PWPR, controlador buck-boost de 4 interruptores |
| Interruptores de potencia | Q1–Q4: MOSFET de canal N de 40 V en LFPAK56 (PSMN3R2-40YLD) |
| Inductor | L1: Pulse PA4344, 4.7 µH |
| Sensado de corriente | R12: shunt Kelvin de 5 mΩ (2512) |
| Condensadores de potencia | Entrada: C2, 100 µF · Salida: C19 y C20, 220 µF · Cerámicos de 10 µF: C4, C14 y C15 |
| Ajuste de VOUT | Divisor de realimentación (R13 49.9 kΩ, R14 1.65 kΩ) y trimmer RV1 |
| Diodos | D1–D3: Schottky B5819W (bootstrap) |
| Conectores | J1 y J2: XT60 (entrada de batería y salida al riel de motores) |

### Mapa de pines (control Rev A)

| Bus / señal | Pines |
|---|---|
| I²C de placa (INA226 `0x40`, PCA9685 `0x41`, BNO085 `0x4A`/`0x4B`) | SDA `IO18` · SCL `IO17` |
| I²C externo (TCA9548A `0x70`) | SDA `IO39` · SCL `IO38` · RESET `IO40` |
| Sensor de corriente ACS725 | `IO9` |
| CAN (500 kbit/s) | TX `IO6` · RX `IO7` |

## Estado del proyecto

| Parte | Estado |
|---|---|
| **Placa de control, Rev A** | ✅ Fabricada y operativa. Autoprueba: **8 pruebas aprobadas, 0 fallas** |
| Placa de potencia (LM5175) | 🔧 Fabricada, en bring-up ([registro](Hardware-Potencia/physical_debug/README.md)). Ya alimentó los motores a 12 V con la entrada barrida de 8 a 24 V |
| Drivers de motor (DRV8874) | ✅ Probados con N20 y Faulhaber a 12 V en el banco de motores ([ensayo](Hardware/physical_debug/11_ensayo_motores_N20_Faulhaber.md)). Temperatura de pistas pendiente |
| BNO085 | ⏸️ Aún no montado en la placa fabricada |
| micro-ROS sobre la placa | ⏳ Siguiente paso |

### Resultados de la placa de control

| Prueba | Resultado medido | Estado |
|---|---|:---:|
| USB-C nativo | Enumera como `303a:1001`; flasheo con hash verificado | ✅ |
| Alimentación | Riel de 3.3 V estable; VBUS de 8 a 22 V con Wi-Fi activo | ✅ |
| Bus I²C | Detecta `0x40` (INA226), `0x41` (PCA9685) y `0x70` (TCA9548A) | ✅ |
| INA226 · V batería | ID `0x5449` leído; Vbus medido = 8.001 V | ✅ |
| PCA9685 · 16 PWM | Escritura y lectura de registros correcta | ✅ |
| ACS725 · corriente | Cero calibrado en 319 mV, sensibilidad 264 mV/A | ✅ |
| CAN · SN65HVD230 | Eco a 500 kbit/s con 0 errores de bus | ✅ |
| Telemetría Wi-Fi | Autoprueba servida en `http://192.168.4.1` | ✅ |

<table>
  <tr>
    <td align="center" width="50%"><img src="docs/img/termografia.jpg" alt="Termografía de la placa de control con VBUS aplicado, máximo 39.6 °C"><br><sub><b>Termografía con VBUS</b> · máx. 39.6 °C</sub></td>
    <td align="center" width="25%"><h1>8/8</h1><sub>pruebas aprobadas en la autoprueba (0 fallas)</sub></td>
    <td align="center" width="25%"><h1>8–22 V</h1><sub>VBUS estable con telemetría Wi-Fi activa</sub></td>
  </tr>
</table>

La termografía con VBUS dio un máximo de 39.6 °C. El detalle del bring-up, con errata y retrabajos incluidos, está en [`Hardware/physical_debug`](Hardware/physical_debug/README.md).

## Inicio rápido

### 1. Flashear el firmware de autoprueba

El firmware vive en [`Hardware/physical_debug/`](Hardware/physical_debug/). La imagen más completa es [`orca_selftest`](Hardware/physical_debug/orca_selftest/orca_selftest.ino).

**Arduino IDE** (Herramientas →):

| Opción | Valor |
|---|---|
| Placa | ESP32S3 Dev Module |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | **Enabled** |
| Flash Mode / Size | QIO 80MHz / 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |

**PlatformIO**: el proyecto de [`platformio_bringup/`](Hardware/physical_debug/platformio_bringup/platformio.ini) ya trae la configuración de la placa.

```bash
cd Hardware/physical_debug/platformio_bringup
pio run -t upload
pio device monitor        # 115200 baudios por el USB nativo
```

### 2. Ver la autoprueba

Cada 5 s la placa corre todas las pruebas y publica el informe por **USB** y por **Wi-Fi**.

1. Conéctate a la red `ORCA-Board` (la clave está en el sketch; cámbiala si vas a usar la placa fuera del laboratorio).
2. Abre **http://192.168.4.1/** para ver el informe en vivo.
3. Opcional: `/status.json` (telemetría y resultados) y `/report.txt` (informe en texto).

### 3. Registrar telemetría desde el PC

```bash
python3 Hardware/physical_debug/wifi_telemetry/orca_monitor.py --csv prueba.csv
```

Solo usa la biblioteca estándar de Python, guarda todo en CSV y avisa si la placa deja de responder o se reinicia.

## Estructura del repositorio

```text
ORCA-Board/
├── Hardware/                  PCB de control (KiCad): esquemáticos, PCB, footprints y gerbers
│   └── physical_debug/        Bring-up de la placa fabricada: documentos 01–09 y firmware de prueba
├── Hardware-Potencia/         PCB de potencia LM5175 (KiCad) y su bring-up
│   ├── Gerbers-Pot/           Archivos de fabricación
│   └── physical_debug/        Protocolo de arranque por etapas y diagnóstico
├── simulacion/                Modelos y simulaciones LTspice del buck-boost
│   ├── scripts/               Análisis y gráficas en Python
│   └── resultados/            Figuras de las corridas
├── examples/                  Firmware de ejemplo en proyectos (brazo por gestos: IMU y servos)
├── Datasheets/                Hojas de datos de los componentes
├── Prints/                    Esquemático y layout de la placa de potencia en PDF/PNG
└── docs/img/                  Imágenes de este README
```

## Simulación de la etapa de potencia

<p align="center">
  <img src="docs/img/layout-potencia-top.png" alt="Layout PCB de la placa de potencia, cara superior, en KiCad" width="50%"><br>
  <sub><b>Layout PCB</b> · placa de potencia, cara superior (KiCad)</sub>
</p>

La carpeta [`simulacion/`](simulacion/) contiene el modelo LTspice del LM5175 y el barrido de **4 esquinas** (corrida del 29-ago) con escalón de carga de 2 A a 8.4 A a los 24 ms, contrastado con los límites de diseño de 8 A continuos y 15 A de pico en el inductor.

<p align="center">
  <img src="simulacion/resultados/c2_4esquinas_vout.png" alt="Tensión de salida en las 4 esquinas, con escalón de carga a los 24 ms" width="80%">
</p>

| VIN | VOUT esperado | VOUT simulado | Desviación | I_L pico | Pin | Pout | Eficiencia |
|---|---|---|---|---|---|---|---|
| 6 V | 7.2 V | 7.199 V | −0.01 % | 13.0 A | 69.4 W | 60.5 W | 87.2 % |
| 12 V | 12 V | 11.999 V | −0.01 % | 11.2 A | 109.0 W | 100.8 W | 92.4 % |
| 22.2 V | 25 V | 24.997 V | −0.01 % | 14.3 A | 219.4 W | 210.0 W | 95.7 % |
| 25.2 V | 7.2 V | 7.199 V | −0.01 % | 10.8 A | 69.2 W | 60.5 W | 87.4 % |

Las cuatro esquinas regulan dentro de ±0.01 % y mantienen el pico del inductor bajo 15 A (el menor margen es 0.7 A, en 22.2 → 25 V). VOUT simulado, Pin, Pout y eficiencia se miden en la ventana de 27 a 28 ms, con el escalón de carga aplicado. Datos completos en [`simulacion/resultados/c2_4esquinas_resumen.csv`](simulacion/resultados/c2_4esquinas_resumen.csv).

> **Nota sobre el barrido de 16 esquinas (28-ago).** Fue una corrida preliminar y se descartó como referencia: los cuatro casos de VIN = 6 V no arrancan (VOUT final de −1.55 V) y en los demás el pico del inductor llega a 34–37 A, por encima del límite de 15 A. Ese export tampoco registra la corriente de entrada, así que no permite calcular Pin ni eficiencia. Sus resultados siguen en [`simulacion/resultados/c1_16esquinas_resumen.csv`](simulacion/resultados/c1_16esquinas_resumen.csv) y su gráfica en [`c1_16esquinas_resumen.png`](simulacion/resultados/c1_16esquinas_resumen.png) solo como registro. La referencia vigente es la de 4 esquinas.


## Documentación

| Qué | Dónde |
|---|---|
| Bring-up de la placa de control | [`Hardware/physical_debug/README.md`](Hardware/physical_debug/README.md) |
| Bring-up de la placa de potencia | [`Hardware-Potencia/physical_debug/README.md`](Hardware-Potencia/physical_debug/README.md) |
| Hojas de datos | [`Datasheets/`](Datasheets/) |
| Esquemático y layout de potencia | [`Prints/`](Prints/) |

## Próximos pasos

- [ ] Montar el BNO085 y poner `BNO085_MOUNTED = true` en la autoprueba.
- [ ] Terminar el bring-up de la placa de potencia.
- [ ] Correr micro-ROS sobre la placa.
- [ ] Montarla en un robot del laboratorio para mover motores y servos con carga real.

## Equipo

Proyecto de la Facultad de Ingeniería de la Universidad de los Andes, presentado en la Semana de la Innovación.

| Autor |
|---|
| Juan Angulo |
| Felipe Gutiérrez |
| Luis Aristizábal |

<details>
<summary><b>Ver el póster de la Semana de la Innovación</b></summary>
<br>
<p align="center">
  <img src="docs/img/poster.jpg" alt="Póster ORCA-Board: control robótico para ROS 2" width="85%">
</p>
</details>

## Licencia

Este proyecto se distribuye bajo la [licencia MIT](LICENSE).
