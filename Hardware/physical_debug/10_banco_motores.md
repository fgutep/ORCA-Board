# 10 — Banco de prueba de motores (U5/U6 DRV8874)

**Fecha:** 20 de noviembre de 2026
**Objetivo:** probar con carga los dos puentes H y cerrar los hallazgos abiertos P3/P4 de [07](07_revision_potencia.md) (pistas VM y OUT2 delgadas en U5/U6).

**Archivos:** [`orca_motor_bench/`](orca_motor_bench/): firmware PlatformIO (`src/main.cpp`). Todo se opera desde el panel web de la placa: control manual, STOP y secuencias de prueba.

## Hardware tal como está en Rev A

Del netlist de `ORCA-BOARD.kicad_sch` y el datasheet del DRV8874:

| Señal | M1 (U5 → J3) | M2 (U6 → J4) | Nota |
|---|---|---|---|
| EN/IN1 | IO16 | IO12 | PWM (LEDC, 20 kHz por defecto, 10 bits) |
| PH/IN2 | IO15 | IO13 | Dirección: alto = adelante (OUT1 → OUT2) |
| nSLEEP | IO14 | IO11 | Pull-up de 10 k (R11/R13). En el esquemático la red se llama `DRVx_PMode`, pero va a nSLEEP |
| PMODE | GND | GND | **Modo PH/EN fijo.** EN = 0 es freno (ambos low-side); "libre" solo se logra con nSLEEP = 0 |
| IMODE | GND | GND | Off-time fijo (25 µs) y **reintento automático** de OCP (2 ms): no hay fallas enclavadas que borrar |
| VREF / IPROPI | 3.3 V / 2.4 kΩ | 3.3 V / 2.4 kΩ | ITRIP ≈ 3.3 / (2.4 k × 450 µA/A) ≈ **3.06 A** por driver |
| nFAULT | solo R10 a 3V3 | solo R12 a 3V3 | ⚠️ **No llega al ESP32** |
| IPROPI | solo R9 a GND | solo R14 a GND | ⚠️ **No llega al ESP32** |

Consecuencias para la prueba:

- **No hay corriente por canal ni aviso de falla.** La única medida es la corriente total de VM con el ACS725 (IO9). Para saber cuánto toma cada motor hay que probarlos uno a la vez (así lo hacen las secuencias).
- Un OCP o un TSD del driver solo se nota como una caída de corriente o un motor que se detiene.
- **VM no se mide en la placa.** El INA226 mide VBUS (J19). Si VM (J20) viene de otra fuente, anotar su voltaje a mano.
- Al encender, el pull-up de nSLEEP despierta los drivers antes de que corra el firmware; EN queda en bajo por su pull-down interno (freno), así que los motores no giran. El firmware pone nSLEEP en bajo en la primera línea de `setup()`.

**Para Rev B:** llevar nFAULT a un GPIO (comparte bien un pin entre los dos drivers, es open-drain) e IPROPI a dos pines ADC1. Quedan libres IO1–IO5, IO8 e IO10 (ADC1).

## Qué mide el banco

La corriente medida es la de **alimentación**, no la del motor. En modo PH/EN, durante el tiempo apagado del PWM la corriente del motor recircula por los low-side y no pasa por el ACS725:

```
I_alim ≈ duty × I_motor        →        I_motor ≈ I_alim / duty
```

El informe de las secuencias muestra la estimación de I_motor solo con duty ≥ 20 %. Las pistas OUT1/OUT2 llevan I_motor; la pista VM lleva pulsos de I_motor (RMS ≈ I_motor × √duty).

VIOUT no tiene filtro RC y el ADC toma lecturas puntuales, así que cada lectura cae en una fase cualquiera del PWM. El firmware toma 4 lecturas por ms con un retardo aleatorio de 0–50 µs entre ellas, para que no se enganchen a la fase del PWM:

| Valor | Qué es |
|---|---|
| `isens_a` | Media de 100 ms (400 lecturas). Es la que dispara `ilim` |
| `isens_max_a` / `isens_min_a` | Máximo y mínimo de las medias de 10 ms desde el último `peaks`. Un pico más corto que 10 ms queda promediado |
| Negativo | El motor devuelve energía a VM (freno regenerativo o inversión). El ACS725 es unidireccional, pero alcanza a leer hasta ≈ −1.2 A |

## Firmware `orca_motor_bench`

### Panel web

Conectarse al AP `ORCA-Board` (clave `orca1234`) y abrir `http://192.168.4.1/`. De arriba abajo:

| Sección | Qué tiene |
|---|---|
| **STOP** | Botón rojo; también la barra espaciadora (fuera de los campos de texto) |
| **HALT** | Aparece cuando hay un HALT activo, con la causa y el botón **Liberar** |
| Telemetría | I media, pico y mínimo, Vbus (J19), cero del ACS725, último evento y respuesta del último comando |
| **Secuencia de prueba** | Prueba, parámetros y botón **Iniciar secuencia**. Debajo: progreso, informe con PASS/WARN/FAIL y enlace para descargar el CSV |
| Control manual | Un control deslizante por motor (−100 a 100 %), botones de duty fijo, freno y libre, ajustes (rampa, ilim, deadman, PWM), zero y reinicio de pico |

Durante una secuencia o un HALT, el control manual y el formulario de secuencias quedan deshabilitados.

### STOP y HALT

**STOP detiene todo y deja la placa en HALT.** STOP deja los dos motores libres (nSLEEP = 0), aborta la secuencia en curso (queda como FAIL en el informe) y **rechaza toda orden de movimiento** (`m`, `brake`, controles deslizantes, iniciar secuencia) hasta pulsar **Liberar** (`release`). Mientras dura el HALT siguen disponibles los comandos que no mueven nada: `coast`, ajustes, `zero`, `peaks`, `status`.

Tres cosas producen el mismo HALT enclavado, con su causa visible en el panel:

| Causa | Cuándo |
|---|---|
| STOP | Botón, barra espaciadora o `stop` por USB |
| Sobrecorriente | `isens_a` (media de 100 ms) > `ilim`. La tarea del ACS725 pone nSLEEP en bajo directamente, sin esperar a `loop()` |
| Deadman | Hay un motor activo y nadie consultó `/status.json` ni mandó comandos en `deadman` ms (por defecto 2000). Pasa si se cierra el panel o se cae el Wi-Fi |

Otras protecciones:

- **Al arrancar**, los dos drivers duermen y no hay HALT.
- **ITRIP del hardware (3.06 A)** sigue activo aunque se suba `ilim`.

### Secuencias de prueba

**Se inician solo con el botón del panel.** Por USB, `run` se rechaza. Corren en una tarea de la placa, así que no dependen del PC más allá de mantener el panel abierto (el panel alimenta el deadman). Mientras una secuencia está en curso, solo se acepta STOP.

| Prueba | Qué hace | Qué marca |
|---|---|---|
| `sweep` | Duty de *paso* a *máx* en ambos sentidos en el canal elegido; asienta *asentar* s y mide *medir* s | WARN si a duty máximo hay < 30 mA (motor desconectado, sin VM o sin conmutar) o si la asimetría adelante/atrás supera el 25 % |
| `step` | Escalón sin rampa desde freno durante *tiempo* (2 s por defecto) | Pico de arranque (media de 10 ms) contra régimen |
| `reverse` | *ciclos* inversiones a ±*duty*, *tiempo* por sentido, con la rampa vigente | WARN si hay corriente negativa > 0.1 A (regeneración hacia VM) |
| `soak` | Marcha continua a *duty* durante *tiempo* (60 s por defecto, máx. 600); informa cada 5 s | Deriva de corriente. La temperatura se mide aparte |
| `both` | Cada motor solo y luego los dos juntos a *duty* | WARN si "juntos" difiere > 25 % de la suma |
| `zero` | Calibra el cero del ACS725 | — |
| `full` | `zero` + `sweep`, `step` y `reverse` en M1 y M2 + `both` (~1.5 min con los valores por defecto) | — |

*rampa* e *ilim* en blanco usan los de la placa; si se llenan, rigen solo durante la secuencia. Un HALT o un error abortan la secuencia y cuentan como FAIL.

**Registro:** la placa guarda una fila cada 100 ms (tiempo, paso, duty de M1 y M2, I media, pico, mínimo, Vbus) hasta 4000 filas (~400 s). **descargar CSV** (`/log.csv`) se habilita al terminar la secuencia: la descarga detiene `loop()` mientras dura. El informe de texto está en `/seq.txt`.

### Comandos

Por USB (115200) o `http://192.168.4.1/cmd?c=<comando>`. El panel usa los mismos.

| Comando | Efecto |
|---|---|
| `m <1\|2\|all> <duty>` | Duty de −100 a 100 %. Despierta el driver y sigue la rampa |
| `brake <canal>` | Duty 0 inmediato con EN = 0 (freno) |
| `coast <canal>` | nSLEEP = 0: puente libre, sin HALT |
| `stop` / `release` | HALT (ver arriba) / borrarlo |
| `ramp <%/s>` | Rampa de duty (por defecto 200 %/s; 0 = escalón). Al invertir el sentido, primero baja a 0 |
| `ilim <A>` | HALT por corriente media de 100 ms (por defecto **2.0 A**, de 0.1 a 9 A) |
| `deadman <ms>` | Por defecto 2000 ms; 0 = apagado. Por USB, usar `deadman 0` para mandar comandos a mano sin que salte |
| `freq <Hz>` | Frecuencia de PWM, de 1 a 50 kHz |
| `zero` | Calibra el cero del ACS725 (con los dos motores libres). Se hace solo al arrancar |
| `peaks`, `status`, `ping`, `help` | Reiniciar picos, estado en JSON, mantener vivo el deadman, ayuda |
| `run <prueba> [clave=valor ...]` | Solo desde el panel |

`/status.json` mantiene los campos de `wifi_telemetry`, así que `orca_monitor.py` también registra Vbus, Isens, reinicios y brownouts mientras corre el banco.

Flasheo: `pio run -t upload` en `orca_motor_bench/` (reemplaza a `orca_selftest`; para volver, flashear ese sketch). Al abrir el puerto serie con pyserial, poner `dtr = rts = False` antes de `open()`; si no, el chip puede quedar en `waiting for download`.

## Procedimiento recomendado

0. **Antes de pasar de ~10 V en VM:** confirmar el voltaje nominal de C9, C12, C8, C10 y los bulk de VM (07, P1).
1. **Fuente de laboratorio en J20 a 12 V, límite de 1 A**, J19 alimentado (o USB), motores **sin carga mecánica**.
2. Conectarse al AP y abrir `http://192.168.4.1/`. Mover cada motor a mano al ±25 %: confirmar el sentido y que "libre"/"freno" se comportan distinto.
3. Probar el STOP: con un motor girando, pulsar STOP (y la barra espaciadora). Confirmar que se suelta, que el panel muestra HALT y que los controles quedan bloqueados hasta **Liberar**.
4. En el panel, secuencia `full` → **Iniciar secuencia**. Al terminar, descargar el CSV y copiar el resumen en la tabla de abajo.
5. **Carga térmica (P3/P4):** con carga mecánica o rotor bloqueado, subir el límite de la fuente y correr `soak` a 1, 2 y 3 A de I_motor, de a un motor, con *ilim* por encima de la I_alim esperada. Medir con cámara térmica o termopar U5/U6, la pista VM (U5.11/U6.11, 0.45/0.40 mm) y OUT2 (U5 0.45 mm, U6 0.35 mm). Con rotor bloqueado a 100 % el driver regula a ITRIP ≈ 3.06 A por chopping, así que I_alim se lee por debajo de I_motor.
6. `both` a la carga más alta que soporte el paso 5.

## Resultados

| Fecha | VM | Motor / carga | Prueba | Resultado |
|---|---|---|---|---|
| 2026-11-20 | 12 V (placa de potencia, Placa 2) | N20 libre, M2 | `sweep` 20–100 % | ✅ I_motor ≈ 0.17 A constante, asimetría ≤ 4 % ([11](11_ensayo_motores_N20_Faulhaber.md)) |
| 2026-11-20 | 12 V (placa de potencia, Placa 2) | Faulhaber libre, M2 | `sweep` 20–100 % | 🟠 Máx. 0.257 A a 60 %, 0.110 A a 100 %: curva no monótona, probable rizado de PWM ([11](11_ensayo_motores_N20_Faulhaber.md), R3) |
| 2026-11-20 | 12 V (placa de potencia, Placa 2) | Faulhaber bloqueado, M2 (2 ensayos) | `sweep` 20–100 % | 🟡 Hasta 1.37 A sostenidos y 2.28 A de pico; bloqueo no rígido; 1.er ensayo cortado ([11](11_ensayo_motores_N20_Faulhaber.md), R4) |
| 2026-11-20 | 12 V (placa de potencia, Placa 2) | Faulhaber en M1 y M2 | Manual, ambos al 100 % | ✅ Pico de 4.246 A (10 ms) sin HALT por `ilim` ([11](11_ensayo_motores_N20_Faulhaber.md), R5) |

Gráficas y tablas desde los CSV del panel: `orca_motor_bench/plot_runs.py` (ver [11](11_ensayo_motores_N20_Faulhaber.md#regenerar-las-gráficas)).
