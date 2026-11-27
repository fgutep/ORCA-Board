# Physical debug — ORCA-BOARD Potencia (LM5175)

Registro del bring-up físico de la PCB de potencia (buck-boost LM5175PWPR, salida ajustable ≈ 6.8–25 V) fabricada y ensamblada por JLCPCB.

**Inicio:** 10 de noviembre de 2026

## Estado actual

| Ítem | Placa 1 | Placa 2 |
|---|---|---|
| Etapa 0 — Inspección visual | ✅ | ✅ |
| Etapa 1 — Mediciones sin energía | 🔴 BOOT2–SW2 = 28 Ω (falla) | ✅ BOOT1–SW1 y BOOT2–SW2 altos |
| Etapa 2 — Energizado con EN a 0 | — | ⏳ Sin registro |
| Etapa 3 — Standby a 4.5 V | ✅ (normal) | ⏳ Sin registro |
| Etapa 4 — Primer arranque a 7.2 V | 🔴 VCC en UVLO, salida ≈ 3 V, ráfagas a ~21 kHz | ✅ Arranca y regula (12.025 V con Vin = 12 V, 23-nov-2026); sin registro por etapa |
| Etapas 5–7 — Ajuste, barrido, carga | — | 🟡 Carga: el 23-nov regula hasta 1.25 A / 15 W (η ≈ 89 %), luego apagados repetidos y deja de arrancar ([04](04_falla_placa2_y_mejoras.md)). El 24-nov, en boost (Vin = 18 V, Vout ≈ 25 V), entrega hasta 198.7 W con η ≈ 87.6 %, y a 12 V entrega 5 A con Vin = 9, 10.5, 11, 13, 14, 16 y 20 V. Corta solo en la zona de transición (Vin ≈ Vout, ≈ 4.8 A a Vin = 12 V). Los MOSFET del lado de Vout (Q3/Q4) llegaron a 180 °C a 200 W ([registro](../pruebas_carga/registro.md)) |
| Causa raíz | LM5175 con la etapa high-side 2 (BOOT2/SW2) dañada internamente | LM5175 con el pin EN dañado: hunde ≈ 90 µA con energía ([04](04_falla_placa2_y_mejoras.md)) |
| Reclamo a JLCPCB | ⏳ Abierto el 10-nov-2026, plazo de 2–3 días para enviar evidencia ([03](03_reclamo_jlcpcb.md)) | — |

## Uso como fuente de VM (20 de noviembre de 2026)

Según el equipo, la placa de potencia usada fue la Placa 2 (la del LM5175 funcional) y alimentó VM (J20) de la placa de control a **12 V** durante el primer ensayo de motores, mientras su entrada se barría en vivo de **8 a 24 V**. Carga: hasta ~1.4 A sostenidos (2 s) y picos de 4.25 A (media de 10 ms) con dos motores Faulhaber. Detalle de la carga en [Hardware/physical_debug/11](../../Hardware/physical_debug/11_ensayo_motores_N20_Faulhaber.md).

*Pendiente:* el registro del barrido de entrada (tensión de salida, temperaturas). No se marca ninguna etapa del protocolo como cumplida hasta tener esos datos.

## Documentos

| Archivo | Contenido |
|---|---|
| [01_diagnostico_placa1.md](01_diagnostico_placa1.md) | Síntoma, mediciones, mecanismo de falla y conclusión de la Placa 1 |
| [02_protocolo_bringup.md](02_protocolo_bringup.md) | Datos del diseño para banco, puntos de medición, lecciones y protocolo de arranque por etapas (0–7) con criterios de paso y de parada |
| [03_reclamo_jlcpcb.md](03_reclamo_jlcpcb.md) | Evidencia y seguimiento del reclamo por el LM5175 defectuoso |
| [04_falla_placa2_y_mejoras.md](04_falla_placa2_y_mejoras.md) | Falla de la Placa 2 en pruebas de carga, layout fabricado contra el datasheet, mejoras para la Rev 2 y verificación con osciloscopio |
| [../pruebas_carga/](../pruebas_carga/README.md) | Script de control de carga y fuente, registro de corridas R01–R06 y datos crudos |
