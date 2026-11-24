# Registro de corridas — placa de potencia LM5175

Codificación: ver [CODIGOS.md](CODIGOS.md). Veredictos: **VÁLIDA** (usar en el reporte), **PARCIAL** (útil con reservas), **DESCARTADA** (no usar).
Montaje común: carga Rigol DL3021, fuente EA PS 10060-170, V_OUT ≈ 12 V (RV1 fijo), 23-nov-2026.

| ID | Archivo (`logs/`) | Comando | Vin | `--imax` | V0 | Veredicto | Observaciones |
|---|---|---|---|---|---|---|---|
| R01 | `20261123_180244_cc` | `cc 0.5 --t 5` | 12 V | 10 | 12.025 V | PARCIAL | Prueba de humo. La carga regula 0.500 A, Vout 11.888 V, Iin 0.7 A. η = 70.7 % no confiable (Iin con resolución 0.1 A a 6 W). |
| R02 | `20261123_180302_steps` | `steps 1 2 4 --dwell 8` | 12 V | 10 | 12.025 V | DESCARTADA | Se perdió la DL3021 del USB a los ~3 s (Iout = 0 con Iset = 1 A, 18 muestras). La fuente quedó encendida y se apagó a mano; `off()` corregido después. |
| R03 | `20261123_180601_plan` | `plan` (vins 9 12 16.8, línea 30 %, soak 480 s) | 12 V nominal | 5 | 12.02 V | PARCIAL (ABORTADA a 95 s) | Plan con Iout máx. 5 A. A y B 0.5 A completos; en B 1.25 A (15 W) estable 32 s (Vout 11.935 V, Iin 1.4 A, η ≈ 89 %) y a t = 94.8 s la placa dejó de conmutar sola: Iin 1.4→0 A con Vin = 12.0 V (fuente no en límite) y Vout cayó a 4.5 V. Abortó el script (piso 80 %); carga y fuente quedaron OFF. Sin fotos térmicas. Causa pendiente (ver nota). |
| R04 | `20261123_181202_plan` | `plan` (repetición de R03) | 12 V nominal | 5 | 12.021 V | PARCIAL (ABORTADA a 56 s) | Mismo síntoma que R03 pero **a 0.5 A (6 W)**, 30 s después de aplicar la carga: Iin 0.7→0 A a t = 55.3 s con Vin = 12.0 V, Vout 11.98 V → 1.1 V en 0.5 s. Abortó el script; carga y fuente OFF. El fallo no depende de una corriente fija: es intermitente. |
| R05 | `20261123_181557_plan` | `plan --etapas CED` | 12 V nominal | 5 | 12.021 V | DESCARTADA (placa no arranca) | Placa se apagó a t = 7.6 s del primer escalón dinámico (3.75 A) y no volvió a arrancar. Todo lo posterior (C resto, E) corrió sobre placa apagada, sin datos útiles. Tras el ciclo de la fuente: Vout = 0.21 V con Vin = 12.0 V → el script se detuvo («La placa no arranca»). Bug del script hallado: el vigilante de Vout no actuaba en tramos de 0.5 s (corregido). Sin soak ni fotos térmicas. |
| R06 | `20261123_182416_plan` | `plan` completo (tras reparar una conexión) | 12 V nominal | 5 | 12.025 V | PARCIAL (detenida en C) | A OK. B 0.5 A: estable 34.6 s (Vout 11.963 V, Iin 0.7 A) y apagado a t ≈ 57 s. Reinicio por ciclo de fuente OK (V0 12.024 V). C: al escalón a 3.75 A (45 W) la placa se apagó en < 1.2 s (Vout 11.79 V → 0 con Iout 2.3 A e Iin = 0 A: dejó de conmutar al aplicar el escalón). 2.º reinicio falló: Vout = 1.51 V con Vin = 12 V → script detenido. La eficiencia de C (99.6 %) es inválida. Patrón: apagados a ~30–35 s de aplicar carga (R03, R04, R06) y al escalón (R05, R06-C). |

**Diagnóstico tras R06 (23-nov-2026, multímetro, Vin = 20.84 V sin carga):** VCC 40 mV, SS 4 mV, FB 0.2 mV, EN 0.61 V (esperado ≈ 5.2 V). Pads: R3 lado VBUS 20.84 V, R3/R2 lado EN 0.61 V, R2 lado GNDA 0 V. Caída en R1 4 mV (≈ 0.4 mA, sin corto interno grueso). Sin energía: R3 = 200 kΩ, EN–GNDA = 55 kΩ subiendo (sano), modo diodo en EN normal. EN no va a ningún conector. → Con energía, el pin EN del LM5175 hunde ≈ 90 µA (equivale a ≈ 6.7 kΩ a GNDA) y lo deja bajo el umbral de 1.23 V: **falla interna activa del LM5175**. Hipótesis consistente con los apagados de R03–R06: fuga en EN que crece con la temperatura hasta deshabilitar el chip. Acción: cambiar el LM5175 (C113356) y repetir etapas 1–4 del bring-up, midiendo EN durante las pruebas de carga.

**Estado de la placa tras R05:** no arranca con Vin = 12 V (Vout 0.21 V). No re-energizar sin inspección (ver protocolo etapas 1–3 en [02_protocolo_bringup.md](../physical_debug/02_protocolo_bringup.md)).

**Nota R03:** apagado espontáneo con Vin = Vout = 12 V (zona de transición buck-boost), a solo 15 W. Hipótesis a descartar con osciloscopio/cámara: hiccup por falso límite de corriente, UVLO de VCC, apagado térmico del LM5175, inestabilidad en la zona de transición. No repetir a mayor corriente hasta identificarla.

## Fotos térmicas
No se toman por ahora (sin cámara disponible, indicado por el usuario el 23-nov-2026). Sin datos de temperatura, el soak solo valida la estabilidad eléctrica.

| Corrida | Momento | Archivo | T máx. [°C] / componente |
|---|---|---|---|
| R03 | D, t = 0 | | |
| R03 | D, mitad | | |
| R03 | D, final | | |
