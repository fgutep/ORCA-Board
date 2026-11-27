# Reporte DRC y reglas de diseño: PCB de potencia ORCA-BOARD

> **Actualización 26-nov-2026.** [`DRC.rpt`](DRC.rpt) se regeneró con KiCad 10.0.1 (`kicad-cli pcb drc --schematic-parity`) sobre el diseño fabricado (gerbers de `jlcpcb/` del 13-oct, 77.50 × 52.02 mm, 169 vías): **0 violaciones, 0 pads sin conectar**. La paridad con el esquemático da 53 advertencias, ninguna eléctrica: 47 `footprint_symbol_field_mismatch` (campo `MANUFACTURER` ausente en las huellas) y 4 `extra_footprint` + 2 `duplicate_footprints` de los 4 agujeros de montaje de las esquinas, que no tienen símbolo. Las secciones siguientes describen la versión del 4-oct (67 × 51.4 mm, 134 vías); las reglas de la §2 son las mismas del `.kicad_pro` actual.

DRC de KiCad del 2026-10-04 22:43, sobre el .kicad_pcb guardado a las 22:30: **0 violaciones, 0 pads sin conectar, 0 errores de huellas.**

## 1. Historial de DRC

El DRC del 2026-09-10 dio 37 violaciones y 102 pads sin conectar. Su estado actual:

| Hallazgo (2026-09-10) | Cantidad | Causa | Estado |
|---|---|---|---|
| Clearance pad-a-pad en U1 (LM5175PWP) | 28 | Paso de 0.65 mm, hueco de 0.20 mm entre pads. Es geometría del paquete. | Resuelto con la regla `escape_fino_U1` (0.15 mm) y el mínimo de 0.20 mm |
| Contorno inválido | 1 | El reporte se generó antes de dibujar Edge.Cuts | Resuelto |
| Altura de texto de serigrafía | 8 | Referencias de 0.39 mm y 0.79 mm, el mínimo es 0.8 mm | Resuelto |
| Pads sin conectar | 102 | Ruteo incompleto | Resuelto |

### Regla personalizada (`ORCA-BOARD.kicad_dru`)

```
(rule "escape_fino_U1"
  (condition "A.insideCourtyard('U1') && B.insideCourtyard('U1')")
  (constraint clearance (min 0.15mm)))
```

Solo aplica entre pads de U1 dentro de su courtyard. Estos pasos pasan con 0.20 mm por el paso del paquete. JLCPCB fabrica desde 0.09 mm de clearance.

## 2. Reglas de placa activas

| Regla | Valor |
|---|---|
| Clearance mínimo | 0.20 mm |
| Ancho de pista mínimo | 0.1998 mm |
| Vía mínima | 0.4 mm de diámetro, 0.3 mm de taladro |
| Anillo anular mínimo | 0.05 mm |
| Distancia de cobre al borde | 0.50 mm |
| Hole-to-hole | 0.25 mm |
| Hole clearance | 0.20 mm |

Todas las vías de la placa son de 0.4/0.3 mm (134).

## 3. Netclasses finales

Quedan 3 netclasses, después de eliminar ANALOG y fusionar SENSE con Default y SWITCH con POWER_HI. Medido sobre el PCB: 332 segmentos, 134 vías (todas 0.4/0.3 mm) y 9 zonas.

| Netclass | Clearance | Ancho | Vía | Nets |
|---|---|---|---|---|
| Default | 0.2 mm | 0.1998 mm | 0.4/0.3 mm | GNDA, FB, COMP, SS, SLOPE, RT, MODE, DITH, EN, CS_P, CS_N, ISNS+, ISNS- |
| GATE | 0.25 mm | 0.3 mm | 0.4/0.3 mm | HDRV1/2, LDRV1/2, BOOT1/2, VCC |
| POWER_HI | 0.3 mm | 2.0 mm | 0.4/0.3 mm | VBUS, /VOUT, /VOUT_PRE, GND, /SW1, /SW2, Net-(Q2-S) |

### Justificación
- **Default:** ancho real de 0.2 mm y clearance mínimo real de 0.20 mm. Las señales de control y sensado no llevan corriente relevante.
- **GATE:** impone 0.25 mm de clearance y las medidas lo cumplen (mínimo real 0.25 mm entre pistas GATE). Las pistas van a 0.3 mm, salvo 1.9 mm de HDRV1 a 0.2 mm en el escape de U1.
- **POWER_HI:** impone 0.3 mm de clearance contra Default y GATE, y las medidas lo cumplen. El ancho de 2.0 mm solo se usa en 19.5 mm de pista.

### Pistas que no coinciden con la netclass
- **POWER_HI (2.0 mm):** casi no hay pistas a ese ancho. VBUS, /VOUT y Net-(Q2-S) son pines de sensado o enlaces cortos (0.2 y 0.3 mm), y los nodos de SW1 y SW2 salen de U1 a 0.3 mm. La corriente de potencia va por zonas de cobre (VBUS, /VOUT, /VOUT_PRE, GND, /SW1, /SW2 y Net-(Q2-S)).
- **Clearance de zonas:** cada zona tiene su propio clearance (0.7 mm, y 0.8 mm en GNDA). Ese valor manda sobre el de la netclass para el relleno.

El DRC de KiCad **no** comprueba el ancho de pista contra la netclass, solo contra `min_track_width`. Por eso los anchos se justifican por diseño y no por el DRC.

El cálculo de clearance real cubre pista contra pista y pista contra vía. No incluye pads. Esos los cubre el DRC de KiCad.

## 4. Estructura de capas

Placa de 4 capas, 1.6 mm, rectángulo de 67 × 51.4 mm (3444 mm²) en Edge.Cuts. Los 47 componentes están en F.Cu.

| Capa | Cobre | Dieléctrico bajo la capa |
|---|---|---|
| F.Cu | 35 µm | Prepreg 0.0994 mm |
| In1.Cu | 15.2 µm | Núcleo 1.265 mm |
| In2.Cu | 15.2 µm | Prepreg 0.0994 mm |
| B.Cu | 35 µm | n/a |

La estructura **no sigue** el esquema clásico Señal / GND / VCC / Señal. Se organizó para tener la mayor cantidad de cobre posible en cada zona: los nets de potencia se repiten en varias capas, en la misma región, y GND rellena el resto en las cuatro capas.

| Capa | Pistas | Zonas (área rellena) | Cobertura de zonas |
|---|---|---|---|
| F.Cu | 448 mm de señal (GNDA, VOUT_PRE, SLOPE, MODE, VBUS, VIN, ISNS-, VCC) | GND 971, VBUS 305, VOUT 283, SW2 177, SW1 171, VOUT_PRE 105, Net-(Q2-S) 80, GNDA 63 mm² | 63 % |
| In1.Cu | Ninguna | GND 1668, VBUS 305, VOUT 284, SW2 177, SW1 170, VOUT_PRE 105, GNDA 130, Net-(Q2-S) 80 mm² | 85 % |
| In2.Cu | 170 mm (VOUT 114, VBUS 21, VCC 13, SW1 11, SW2 11) | GND 1435, VBUS 305, VOUT 284, SW2 177, SW1 170, VOUT_PRE 105, GNDA 129 mm² | 76 % |
| B.Cu | 280 mm de señal (GNDA, Net-(Q2-S), RT, BOOT1/2, HDRV2, DITH, EN) | GND 2393, GNDA 130 mm² | 73 % |

### Cómo está repartido
- **Nets de potencia (VBUS, VOUT, VOUT_PRE, SW1, SW2):** zonas de la misma área (por ejemplo VBUS 305 mm²) en F.Cu, In1.Cu e In2.Cu. Apilarlas multiplica la sección de cobre y las vías las ponen en paralelo. B.Cu no tiene zonas de potencia.
- **Net-(Q2-S) (sensado de corriente):** zona de 80 mm² solo en F.Cu e In1.Cu.
- **GND:** zona en las cuatro capas. In1.Cu es la capa con más cobre (1668 mm²).
- **GNDA:** zona en las cuatro capas (63 a 130 mm²). Está separada de GND y solo se une en R15 (jumper de 0 Ω).
- **In1.Cu:** no tiene pistas, solo zonas.
- **Señales:** se rutean en F.Cu, B.Cu y algo en In2.Cu.

### Continuidad de los planos de referencia
Las zonas de GND de las capas internas están partidas: In1.Cu tiene 4 piezas (950, 551, 156 y 10 mm², 48 % del área de la capa) e In2.Cu tiene 5. Eléctricamente están unidas por las otras capas (el DRC da 0 pads sin conectar). Para el retorno de las señales importa qué cobre hay justo debajo de cada pista, a 0.1 mm en la capa vecina (In1.Cu para F.Cu, In2.Cu para B.Cu):

| Señal | Cobre debajo |
|---|---|
| FB, COMP, CS_P, CS_N, ISNS+, ISNS- (F.Cu) | 95 a 99 % GND o GNDA |
| MODE, SLOPE, DITH, EN, RT, SS | 70 a 90 % GND, el resto GNDA o vacío |
| Gate drive en F.Cu (HDRV, LDRV) | 36 a 67 % GND, 22 a 41 % vacío |
| Gate drive en B.Cu (BOOT1/2, HDRV2, LDRV2) | 39 a 70 % GND, 30 a 61 % vacío |
| HDRV1 en B.Cu (12 mm) | 0 % GND, 65 % sobre cobre de potencia |
| Zonas SW1 y SW2 | Ninguna señal de control o gate pasa sobre ellas |

- Las señales sensibles (FB, COMP, sensado de corriente) están bien referenciadas.
- El punto débil son las pistas de gate drive, sobre todo en B.Cu. Donde debajo hay vacío o cobre de otra red, el retorno da un rodeo y el lazo se agranda (más inductancia, más ringing y EMI). El caso más claro es HDRV1 en B.Cu.
- GND y GNDA solo se unen en R15. Una señal cuyo retorno cruza de una zona a otra pasa por ese punto. Las señales analógicas ya van sobre GNDA.
- La medición cubre solo la geometría, no la forma de onda real. Las pistas de In2.Cu cuentan como vacío en las pistas de B.Cu, así que ese porcentaje queda algo inflado.

### Peso del cobre
Se pide al fabricante el cobre del stackup: 1 oz en las capas externas y 0.5 oz en las internas (1 / 0.5 / 0.5 / 1 oz). El handoff estima la corriente de pista con cobre de 2 oz; esa estimación hay que rehacerla con 1 oz.

### Áreas
Las áreas salen de los rellenos guardados en el archivo y no descuentan pads ni vías dentro de cada zona. Hay que rellenar todas las zonas antes de generar los gerbers.

## 5. Pendientes antes de fabricar

1. **Vías 0.4/0.3 mm (anillo de 0.05 mm).** Se eligieron por recomendación de un diseñador de PCB, que indicó usar el mínimo de JLCPCB. El handoff (§105 y §109) todavía lista 0.5/0.3 mm y anillo de 0.1 mm, así que esa sección está desactualizada. Antes de fabricar, confirmar con la página de capacidades de JLCPCB o con su revisión DFM al subir los gerbers.
2. **Confirmar `min_hole_clearance` de 0.20 mm.** Cambió desde 0.25 mm. Sigue sobre el piso de fábrica.
3. **Checks que el DRC ignora.** Courtyard faltante, vía no centrada en pista, perfil de tuning, filtros de huella y tipo de componente.
