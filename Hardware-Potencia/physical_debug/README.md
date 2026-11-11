# Physical debug — ORCA-BOARD Potencia (LM5175)

Registro del bring-up físico de la PCB de potencia (buck-boost LM5175PWPR, salida ajustable ≈ 6.8–25 V) fabricada y ensamblada por JLCPCB.

**Inicio:** 10 de noviembre de 2026

## Estado actual

| Ítem | Placa 1 | Placa 2 |
|---|---|---|
| Etapa 0 — Inspección visual | ✅ | ✅ |
| Etapa 1 — Mediciones sin energía | 🔴 BOOT2–SW2 = 28 Ω (falla) | ✅ BOOT1–SW1 y BOOT2–SW2 altos |
| Etapa 2 — Energizado con EN a 0 | — | ⏳ Pendiente de registrar |
| Etapa 3 — Standby a 4.5 V | ✅ (normal) | ⏳ |
| Etapa 4 — Primer arranque a 7.2 V | 🔴 VCC en UVLO, salida ≈ 3 V, ráfagas a ~21 kHz | ⏳ |
| Etapas 5–7 — Ajuste, barrido, carga | — | ⏳ |
| Causa raíz | LM5175 con la etapa high-side 2 (BOOT2/SW2) dañada internamente | — |
| Reclamo a JLCPCB | ⏳ Abierto el 10-nov-2026, plazo de 2–3 días para enviar evidencia ([03](03_reclamo_jlcpcb.md)) | — |

## Documentos

| Archivo | Contenido |
|---|---|
| [01_diagnostico_placa1.md](01_diagnostico_placa1.md) | Síntoma, mediciones, mecanismo de falla y conclusión de la Placa 1 |
| [02_protocolo_bringup.md](02_protocolo_bringup.md) | Datos del diseño para banco, puntos de medición, lecciones y protocolo de arranque por etapas (0–7) con criterios de paso y de parada |
| [03_reclamo_jlcpcb.md](03_reclamo_jlcpcb.md) | Evidencia y seguimiento del reclamo por el LM5175 defectuoso |
