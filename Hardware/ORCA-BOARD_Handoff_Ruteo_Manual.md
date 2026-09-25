# ORCA-BOARD / Carga Luis — Handoff para ruteo manual

**Fecha:** 14 de septiembre de 2026
**Estado:** placement final, netlist corregido, ruteo pendiente (se abandona el autorouter)
**Archivo:** `C:\Users\Lenovo\Documents\Universidad\2026-20\ORCA-Board\ORCA-Potencia\ORCA-Board\Hardware\ORCA-BOARD.kicad_pcb`

---

## 1. Por qué se abandona KiCadRoutingTools

Se intentó rutear con el plugin (GUI y CLI). Los pasos de potencia se colgaban: una traza de 4 mm con clearance SWITCH de 0.8 mm exige un corredor libre de **5.6 mm**, que en una placa de 78 × 48.5 mm con 47 componentes prácticamente no existe. El router exploraba cientos de miles de caminos sin cerrar y KiCad quedaba en "no responde".

Bajar a 2.5 mm mitigaba el problema pero no lo resolvió de forma confiable. **Se rutea a mano.**

Lo que sí sirvió del plugin y conviene conservar:
- `list_nets.py` para verificar netclasses, pads por componente y pares diferenciales.
- `check_drc.py`, `check_connected.py`, `check_pads.py`, `check_orphan_stubs.py` para verificación final.
- Su análisis encontró dos errores de netlist bloqueantes (ya corregidos, §2).

Ubicación de los scripts:
```
C:\Users\Lenovo\Documents\KiCad\10.0\3rdparty\plugins\com_github_drandyhaas_kicadroutingtools\py_router
```

Comando base de verificación (correr desde esa carpeta):
```powershell
python list_nets.py "C:\Users\Lenovo\Documents\Universidad\2026-20\ORCA-Board\ORCA-Potencia\ORCA-Board\Hardware\ORCA-BOARD.kicad_pcb" --design-rules
```
Flags que existen: `--design-rules`, `--diff-pairs`, `--power`, `--component <REF> --pads`.
(`--pattern` **no existe** — devuelve el resumen genérico sin filtrar.)

---

## 2. Correcciones de netlist ya aplicadas

| # | Problema | Estado |
|---|---|---|
| 1 | `CS`/`CSG` renombrados a `CS_P`/`CS_N` solo del lado de U1 → redes de un solo pad, sensado de corriente sin conexión | **Corregido.** C17 ahora muestra pad 1 = `/CS_N`, pad 2 = `/CS_P`. Total de redes bajó de 34 a 32. |
| 2 | U1.2 (VIN) sin camino desde VBUS: la cadena `D1 → R1 → VIN` colgaba de `/VCC`, que U1 genera *a partir de* VIN (lazo cerrado sin fuente) | **Corregido.** D1 ánodo ahora en `VBUS`. Cadena final: `VBUS → D1 → R1 → U1.2 (VIN)`, con `C1 → GNDA`. |
| 3 | Polaridad de ISNS: `/ISNS+` filtrado desde `/VOUT` (R11) y `/ISNS-` desde `/VOUT_PRE` (R7). La corriente por R8 fluye VOUT_PRE→VOUT, así que la entrada + normalmente va del lado de VOUT_PRE | **PENDIENTE DE VERIFICAR** contra el datasheet antes de fabricar. |

El datasheet confirma que D1 es correcto conceptualmente: con BIAS externo (pin 24 en `/VOUT`), TI pide un diodo de bloqueo entre el riel de entrada y VIN para evitar conducción inversa cuando VIN < VCC.

---

## 3. Estado actual de la placa

**Geometría:** 4 capas, 78 × 48.5 mm, 47 componentes, 32 redes. Todo SMD en F.Cu.
**Cobre exterior:** 2 oz (elegido a propósito). Dieléctricos en los valores por defecto de KiCad — no importa, nada requiere impedancia controlada.

**Stack:**
| Capa | Uso |
|---|---|
| F.Cu (L1) | Componentes y ruteo principal |
| In1.Cu (L2) | Plano sólido de GND (0.1 mm bajo la etapa de potencia → mínimo área de lazo) |
| In2.Cu (L3) | Ruteo / isla GNDA |
| B.Cu (L4) | Ruteo / isla GNDA |

**Ya hecho:**
- Contorno en Edge.Cuts dibujado.
- Placement final de todos los componentes.
- Serigrafía reposicionada automáticamente con el plugin `kicad-auto-silkscreen` (CGrassin). **Volver a correrlo al terminar el ruteo** — evita solapamiento con el cobre nuevo.
- Zona GNDA dibujada en **In1.Cu, In2.Cu y B.Cu** (deliberadamente **NO** en F.Cu: ahí el PowerPAD de U1 ya es el cobre de GNDA, y una zona extra chocaría con los pines vecinos que no son GNDA). Rellenada.
- `/ISNS±` ruteado en F.Cu, marcado como protegido — **pero ambos pads de C18 siguen desconectados**.
- Stubs cortos desde pines de U1 en GND, VBUS, VIN, MODE, LDRV2 y VCC.
- Pour de GND en In1.Cu ejecutado como paso 1 del plan del plugin — **verificar en qué estado quedó**.

**Pendiente de confirmar:** si el pour de GND se tragó la isla de GNDA en In1.Cu (ver §6).

---

## 4. Netclasses

| Clase | Clearance | Ancho | Vía |
|---|---|---|---|
| Default | 0.2 | 0.2 | 0.6 / 0.3 |
| ANALOG | 0.25 | 0.25 | 0.5 / 0.3 |
| SENSE | 0.3 | 0.3 | 0.5 / 0.3 |
| GATE | 0.4 | 0.5 | 0.5 / 0.3 |
| POWER_HI | 0.5 | 4.0 | 0.6 / 0.3 |
| SWITCH | 0.8 | 4.0 | 0.6 / 0.3 |

### Problema abierto: los patrones no calzan

Los nombres reales de las redes llevan prefijo `/` (`/SW1`, `/VOUT`, `/HDRV1`…). Los patrones escritos sin la barra **no coinciden con nada**. Solo `VBUS` y `GND` funcionan, por ser redes globales sin prefijo.

**Verificación:** en Board Setup → Net Classes, clic en una fila y leer el panel derecho "Nets matching". Si sale vacío, no calza. Alternativa: pestaña Route del plugin → casilla "Separate by net class".

**Arreglo:** usar comodines que funcionen con o sin prefijo: `*SW1`, `*VOUT`, `*HDRV*`, etc.

**Patrones faltantes detectados:**
- `*VOUT_PRE` → POWER_HI (lleva corriente de potencia, no tenía fila)
- `Net-(Q2-S-Pad1)` → POWER_HI (red autogenerada entre FETs, 8 pads, corriente real)
- `*BOOT*` → GATE (mismo lazo rápido que HDRV/LDRV)
- `*VCC` → GATE
- `*CS_P`, `*CS_N` → SENSE
- `*ISNS*` → SENSE
- Analógicas (`*FB`, `*COMP`, `*SS`, `*SLOPE`, `*RT`, `*DITH`, `*MODE`, `*EN`) → ANALOG
- La fila `VIN` → POWER_HI probablemente tampoco calza (la red real es `Net-(U1-VIN)`). Esa red solo alimenta el pin de control, poca corriente: puede vivir en Default sin problema.

---

## 5. Board Constraints y pisos de fabricación

**DRC-enforced:** min_clearance 0.2 · min_track_width 0.2 · min_via_diameter 0.5 · min_via_annular_width 0.1 · min_hole_to_hole 0.25 · min_hole_clearance 0.2 · min_copper_edge_clearance 0.5

> **Revisar:** `min_hole_clearance` cambió de 0.25 a 0.2 en algún momento de la sesión, posiblemente sin intención, mientras se editaban netclasses. No es grave (sigue sobre el piso de fábrica) pero conviene confirmar que fue deliberado.

**Piso de fabricación (JLCPCB, tier standard):** vía 0.5/0.3 · clearance 0.09 · hole-to-hole 0.25 · borde 0.5 · track 0.0762 (fine-pitch)

**Flags de verificación final sugeridos por la herramienta:**
```
check_drc.py --clearance 0.09 --hole-to-hole-clearance 0.25 --board-edge-clearance 0.5
```

---

## 6. Zonas: prioridad GNDA vs GND

**Las tierras no se unen eléctricamente** — GND y GNDA son redes distintas y KiCad siempre mantiene el clearance entre zonas de redes diferentes. R15 (jumper 0 Ω) sigue siendo el único punto de unión.

**El riesgo real** es que el pour de GND le gane el área a la isla de GNDA en In1.Cu por prioridad de relleno. No es un corto, pero se pierde la separación justo bajo la sección analógica de U1 — y In1 es precisamente la capa que importa.

**En KiCad 10 la prioridad no es un campo numérico: es el orden de la lista** en el Zone Manager. Más arriba = mayor prioridad.

Procedimiento:
1. `View → Panels → Zone Manager` (no está en el menú View directo).
2. Seleccionar GNDA, subirla al tope con el botón `↟`.
3. Marcar **"Refill zones"** abajo a la izquierda.
4. OK.
5. Confirmar en la previsualización inferior (pestañas In1.Cu / In2.Cu / B.Cu) que la isla sigue viva.

**Propiedades de la zona GNDA a corregir** (si no se alcanzó a hacer):
- Clearance: **0.2** (estaba en 0.5; GNDA es clase Default)
- Pad connections: **Solid** (estaba en Thermal reliefs; el thermal relief es para soldadura manual de THT, aquí interesa mínima inductancia de retorno)
- Minimum width 0.25, Remove islands: Always, Corner smoothing: None → correctos
- Hatched fill: **desactivado** (relleno sólido)

---

## 7. U1 (LM5175, HTSSOP-28 + EP) — pinout y el problema de paso fino

### Pinout verificado
| Pin | Red | Pin | Red |
|---|---|---|---|
| 1 | /EN | 15 | /CS_N |
| 2 | Net-(U1-VIN) | 16 | /CS_P |
| 3 | VBUS | 17 | PGOOD (sin conectar) |
| 4 | /MODE | 18 | /SW2 |
| 5 | /DITH | 19 | /HDRV2 |
| 6 | /RT | 20 | /BOOT2 |
| 7 | /SLOPE | 21 | /LDRV2 |
| 8 | /SS | 22 | GND |
| 9 | /COMP | 23 | /VCC |
| 10 | GNDA | 24 | /VOUT |
| 11 | /FB | 25 | /LDRV1 |
| 12 | /VOUT | 26 | /BOOT1 |
| 13 | /ISNS- | 27 | /HDRV1 |
| 14 | /ISNS+ | 28 | /SW1 |

Pin 29 = EP (GNDA), con 22 vías térmicas. **No necesita fanout**: no son lands que haya que escapar, son las vías del PowerPAD.

### El problema de paso fino

Paso 0.65 mm − pad ~0.40 mm = **~0.25 mm de hueco físico** entre pads vecinos. Cuando dos pines vecinos pertenecen a netclasses distintas, KiCad exige el **mayor** de los dos clearances. Eso hace imposibles estos dos grupos:

- **VCC(23) – GND(22) – LDRV2(21):** LDRV2 es GATE → 0.4 mm requeridos, 0.25 mm disponibles.
- **Net-(U1-VIN)(2) – VBUS(3) – MODE(4):** VBUS es POWER_HI → 0.5 mm requeridos.

No es un fallo del router: con esas reglas no existe camino, sin importar el ancho de la traza (el clearance se mide independiente del ancho).

### Solución: escape fino (neck-down)

Para cada uno de esos 6 pines, un stub cortísimo con clearance mínimo, y ancho completo de netclass solo después de salir de la fila:

1. Ancho de pista: **0.08 mm** (el hueco de 0.25 mm solo alcanza para ~0.09 mm de clearance a cada lado más una pista angosta en medio).
2. `Route → Interactive Router Settings`: Mode = **Highlight Collisions**, marcar **"Allow DRC violations"**.
3. Tecla `X` sobre cada pad, salir en línea recta, terminar el segmento **en el aire** — el criterio es pasar la punta física de los pads vecinos, no solo el hueco lateral. El pad del LM5175 es más largo que el hueco entre pines; juzgarlo con zoom.
4. Clic derecho sobre el segmento → Properties → si existe "Clearance override", ponerlo en **0.09 mm** (deja el tramo legítimamente válido en vez de tolerado).
5. Si no existe ese campo: `Inspect → Design Rules Checker`, clic derecho sobre cada violación → "Exclude this violation", y **anotar el motivo** para no olvidarlo.
6. **Apagar "Allow DRC violations"** al terminar los 6. No dejarlo activo para el resto del trabajo.

### Taps de sensado — rutear finos a mano (~0.25 mm)

Estos pines son de sensado, no llevan corriente, pero pertenecen a redes de potencia. Si se rutean con el ancho de su netclass, meten trazas gruesas atravesando el área del controlador:

- **U1.3** (VISNS, en VBUS) — el stub existente desde U1.3 muere en `y = 108.9`
- **U1.12** y **U1.24** (VOSNS / BIAS, en VOUT)
- **U1.18** y **U1.28** (SW)

Rutearlos delgados hasta su pad de potencia correspondiente antes de rutear el resto de esas redes.

---

## 8. Orden de ruteo manual recomendado

1. **Lazo de potencia** (objetivo: **< 1 cm²**)
   `C14 → QH1 → QL1 → R12 → PGND` y su espejo de salida `C11/C12 → QH2 → QL2 → R12`.
   Router interactivo, ancho 2.5 mm, clearance SWITCH/POWER_HI. **Medir el lazo al terminar.**
2. **Escapes finos de U1** — los 6 stubs de §7.
3. **Taps de sensado** — los 5 de §7.
4. **SW1 / SW2 / Net-(Q2-S-Pad1)** — nodos de mayor dv/dt. Cortos, y **que no pasen bajo el área analógica de U1**.
5. **VBUS / VOUT / VOUT_PRE** — 2.5 mm.
6. **Gate drive:** BOOT1/2, HDRV1/2, LDRV1/2 — 0.5 mm, pegados a su nodo de conmutación respectivo. Separación mínima de 15 mm entre driver y FET; BOOT con lazo < 10 mm.
7. **Bias:** VCC, Net-(U1-VIN), Net-(D1-PadC) — 0.5 mm.
8. **Pares diferenciales:** CS_P/CS_N e ISNS+/ISNS- — paralelos, separación constante ~0.25 mm desde los bordes internos de los pads de R12. **Prohibido cruzar SW1/SW2 o gate drive.** Terminar de conectar los pads de C18 (ISNS quedó a medias).
9. **GNDA** — pads fuera de la isla: C1, R2, R4, C8, RV1 — 0.4 mm.
10. **Analógicas:** FB, COMP, SS, SLOPE, RT, DITH, MODE, EN — 0.25 mm.
11. **Rellenar zonas** (`B`) y verificar prioridad GNDA (§6).
12. **Re-correr `kicad-auto-silkscreen`** para reposicionar referencias sobre el cobre nuevo.

### Sobre los anchos

2.5 mm sobre 2 oz ≈ **10 A con 20 °C de elevación**, que es el punto de operación. Las netclasses dicen 4 mm; es más conservador pero cuesta mucho espacio de ruteo. 2.5 mm es eléctricamente suficiente — si se usa, ajustar la netclass para que el DRC no reporte falsos positivos.

---

## 9. Verificación final

Desde `py_router`, con la placa **cerrada en KiCad**:

```powershell
python check_drc.py "<ruta>\ORCA-BOARD.kicad_pcb" --clearance 0.09 --hole-to-hole-clearance 0.25 --board-edge-clearance 0.5
python check_connected.py "<ruta>\ORCA-BOARD.kicad_pcb"
python check_pads.py "<ruta>\ORCA-BOARD.kicad_pcb"
python check_orphan_stubs.py "<ruta>\ORCA-BOARD.kicad_pcb"
```

`check_drc.py` **excluye por diseño el clearance pad-a-pad dentro de una misma huella** — las violaciones de los pines de U1 que reporta el DRC nativo de KiCad no aparecen ahí. Es geometría fija de librería, no un defecto introducido por el ruteo.

**Además, revisar a mano:**
- Área del lazo de potencia < 1 cm²
- Isla de GNDA viva en In1.Cu, con R15 como único punto de unión
- Ningún nodo de conmutación pasando bajo la sección analógica de U1
- Cobre a borde ≥ 0.5 mm

---

## 10. Pendientes abiertos

| Prioridad | Item |
|---|---|
| Alta | Verificar polaridad de ISNS contra el datasheet (§2, punto 3) |
| Alta | Arreglar patrones de netclass con comodines (§4) |
| Alta | Confirmar estado de la isla GNDA en In1.Cu tras el pour (§6) |
| Media | Corregir propiedades de la zona GNDA: clearance 0.2, pad connections Solid (§6) |
| Media | Confirmar si `min_hole_clearance` 0.2 fue deliberado (§5) |
| Media | Footprint del LM5175: corrección de pads 0.45 → 0.40 mm debe vivir **en la librería**, no solo en el board — un `Update Footprints from Library` la revierte silenciosamente |
| Baja | Reposicionar el campo de referencia en los footprints custom (PA4344, LM5175) dentro del Footprint Editor, para que futuros "reset text positions" partan de un buen default |
| Baja | PGOOD (U1.17) sin conectar — confirmar que es intencional |

---

## 11. Notas operativas de KiCad (tropiezos de esta sesión)

- **Selection Filter:** panel abajo a la derecha. Si todas las casillas se desmarcan (fácil de tocar con un atajo), no se puede seleccionar nada y parece que KiCad está colgado. Marcar "All items".
- **Update Footprints from Library:** la sincronización de geometría contra la librería ocurre siempre, sin importar los checkboxes. Marcar solo "Update/reset text positions" y **desmarcar** "clearance overrides" y "3D models", que vienen activos por defecto.
- **Zone Manager:** está en `View → Panels`, no en el menú View directo.
- **Prioridad de zona:** no hay campo numérico en las propiedades de la zona; es el orden de lista en el Zone Manager.
- **PowerShell y scripts:** `Set-ExecutionPolicy -Scope Process Bypass` antes de ejecutar `.ps1` (solo afecta esa ventana).
- **Scripts del plugin:** siempre cerrar la placa en KiCad antes de correrlos — KiCad sobrescribe el `.kicad_pro`/`.kicad_pcb` al guardar y revierte los cambios del script.
