# 03 — Reclamo a JLCPCB por el LM5175 de la Placa 1

**Fecha de apertura:** 10 de noviembre de 2026 (chat de soporte en la web de JLCPCB)

## Seguimiento

| Fecha | Evento |
|---|---|
| 10-nov-2026 | Chat con soporte. Piden fotos/videos, descripción detallada, cantidad defectuosa y tasa de defecto, y otra información útil |
| 10-nov-2026 | Soporte indica que el equipo de posventa enviará un correo para el Quality Complaint. Plazo para enviar la información: **~2–3 días** |
| — | Pendiente: responder el correo de posventa con la evidencia |

## Evidencia a enviar

- [ ] Número de pedido y total de placas del pedido (tasa de defecto = 1 / total).
- [x] Foto con cámara térmica a 7.2 V / 100 mA: el LM5175 es el único componente caliente.
- [ ] Fotos con lupa del LM5175 y de la placa.
- [ ] Capturas del osciloscopio (ráfagas de ~21 kHz en SW1/SW2).
- [ ] Tabla de mediciones ([01](01_diagnostico_placa1.md)).
- [ ] Medición de la huella vacía pad 20 ↔ 18 y del chip suelto (refuerza el caso).
- [ ] Comparación con la Placa 2 (BOOT–SW normales).

## Descripción enviada (resumen)

El LM5175PWPR (LCSC C113356) de la Placa 1 es defectuoso: salida en ~3 V a 7.2 V de entrada, trimmer sin efecto, chip caliente sin carga, conmutación en ráfagas y BOOT2–SW2 ≈ 28 Ω en ambas polaridades (BOOT1–SW1 normal). La Placa 2 del mismo pedido está en buen estado. Cantidad defectuosa: 1 placa (1 LM5175).
