# 07 — Revisión de potencia: VBUS, buck 5 V, 3V3 y VM

**Fecha:** 10 de noviembre de 2026
**Método:** netlist de `ORCA-BOARD.kicad_sch` + geometría de `ORCA-BOARD.kicad_pcb` (pistas, zonas rellenas, vías) + DRC completo, contra los datasheets de `Datasheets/` (TPS54331, AMS1117, DRV8874, INA226, ACS725).

## Arquitectura

```
J19 XT60 (VBUS, 2S–6S) ──┬── U11 INA226 (Vbus)
                         └── U8 TPS54331 buck ── L1 15 µH ── 5V_SERVOS ──┬── J6–J10 servos
                                                                         └── D3 SS54 ─┐
J1 USB-C (5VUSB) ── D2 SS54 ──────────────────────────────────────────────────────────┴── /5V ──┬── J14/J15 (5 V I2C)
                                                                                                └── U3 AMS1117 ── +3V3
J20 XT60 (VM) ── U12 ACS725 (Isens) ── VMout ──┬── U5 DRV8874 ── J3
                                               └── U6 DRV8874 ── J4
```

## Resumen

| # | Severidad | Hallazgo |
|---|---|---|
| P1 | 🔴 Antes de probar | **Voltaje nominal de los condensadores en VBUS/VM desconocido.** En el BOM, Cin1 comparte línea con C2/C5 (rieles de 3.3/5 V) y Chf1 con C_5 (soft-start). Si se montó la misma pieza, puede ser de 10–16 V |
| P2 | 🔴 Antes de probar | **VBUS solo tiene cerámicos (10 µF + 10 nF + 0.1 µF).** Enchufar en caliente una batería de 4S–6S puede producir un sobrepico de hasta ~2× Vbat, por encima de los 30 V absolutos del TPS54331 |
| P3 | 🟠 | Pines VM de U5/U6 **fuera de la zona VMout**: alimentados por pistas de 0.45/0.40 mm (~7 mm). Con ITRIP ≈ 3.1 A por driver se calientan |
| P4 | 🟠 | Salidas OUT2 de los motores en 0.45 mm (U5, ~10 mm) y 0.35 mm (U6, ~3.6 mm). OUT1 en 1.5 mm |
| P5 | 🟠 | Servos y lógica comparten el buck (límite ≥ 3.5 A). Un bloqueo de servos puede tumbar 5V_SERVOS → /5V → brownout del ESP32 |
| P6 | 🟡 | EN del TPS54331 a ~4.5 V con 6S lleno (máx. absoluto 5 V); llega a 5 V con VBUS ≈ 27.9 V |
| P7 | 🟡 | Solo con USB, el margen del AMS1117 es justo (~1.1–1.2 V con picos de Wi-Fi) |
| P8 | 🟡 | Inductancia L1 y condensadores bulk sin número de parte: verificar Isat de L1 ≥ ~4 A |
| — | ✅ | Divisor de salida, UVLO, soft-start, compensación, diodo de rueda libre, OR de 5 V con diodos, DRC sin errores de potencia |

---

## Buck U8 TPS54331 (VBUS → 5V_SERVOS)

| Parámetro | Componentes | Cálculo | Veredicto |
|---|---|---|---|
| Rango de entrada | — | 3.5–28 V operativo, **30 V máximo absoluto**. 6S lleno = 25.2 V | ✅ con 2.8 V de margen hasta el máximo operativo |
| Voltaje de salida | R_5 10.2 k / R_6 1.91 k | 0.8 × (1 + 10.2/1.91) = **5.07 V** (4.89–5.25 V por la tolerancia de Vref) | ✅ |
| UVLO por EN | R_1 200 k / R_2 42.2 k | Arranca **6.97 V**, para **6.37 V** (hasta 7.55/6.95 V con el EN máximo de 1.35 V) | ✅ corta 2S a ~3.2 V/celda |
| EN con 6S lleno | — | 25.2 × 42.2/242.2 + 4 µA × 34.9 k ≈ **4.53 V** | 🟡 límite de 5 V con VBUS ≈ 27.9 V (P6) |
| Soft-start | C_5 10 nF | 10 nF × 0.8 V / 2 µA = **4 ms** | ✅ (1–10 ms recomendado) |
| Bootstrap | C_4 0.1 µF | — | ✅ |
| Rizado en L1 | L1 15 µH, 570 kHz | 0.47 A p-p a 25.2 V, 0.34 A a 12 V | ✅ |
| Ciclo de trabajo máx. | 90 % mín. | Se sale de regulación por debajo de ~6.6 V con 3 A (2S casi vacío): la salida baja a ~0.9 × Vin | 🟡 aceptable, el UVLO corta poco después |
| Tiempo mínimo de encendido | 160 ns máx. | A 25.2 V: D/fsw ≈ 390 ns | ✅ |
| Compensación | R_3 16.2 k, C_6 1.8 nF, C_7 120 pF | Cruce ≈ 10 kHz (44 µF nominal), cero 5.5 kHz, polo 82 kHz | ✅ razonable |
| Diodo de rueda libre | D_2 B340 (40 V / 3 A) | Corriente media a 6S y 3 A ≈ 2.3 A → ~1.2 W en el SMC | ✅ (se calienta a plena carga) |
| Límite de corriente | interno | **≥ 3.5 A** | Ver P5 y P8 |
| Condensador de entrada | Cin1 10 µF 1206 + Chf1 10 nF | Voltaje nominal desconocido | 🔴 P1 |
| Condensador de salida | Cout1 + Cout2 22 µF 1206 | — | ✅ |

### Layout del buck

| Lazo / pista | Medición | Veredicto |
|---|---|---|
| VIN (U8.2) ↔ Chf1 | 2.8 mm | ✅ |
| VIN (U8.2) ↔ Cin1 | 5.7 mm | 🟡 aceptable |
| GND de U8 ↔ GND de Cin1 | 11.1 mm (por plano) | 🟡 lazo caliente algo grande |
| PH ↔ D_2 / L1 | 6.3 / 7.3 mm, zona de cobre en F.Cu | ✅ |
| Pads de PH en la zona | Alivio térmico con radios de 0.5 mm | 🟡 funciona; en Rev B mejor conexión sólida |
| Pista VBUS | 0.7 mm × 40 mm + 1.5 mm × 22 mm, 35 µm | ✅ ~2.5 A para ΔT = 20 °C; la entrada del buck con 2S a plena carga ronda 2.4 A |
| Divisor de realimentación | Toma de 5V_SERVOS a ~20 mm de Cout1; VSENSE a 7–8 mm de R_5/R_6 | ✅ |

## Rieles de 5 V y 3.3 V

| Punto | Valor estimado | Veredicto |
|---|---|---|
| 5V_SERVOS | 5.07 V | ✅ pistas de 2 mm, zonas en F.Cu e In2, 18 vías |
| /5V desde la batería | 5.07 − ~0.4 V (D3) ≈ **4.65 V** | ✅ |
| /5V desde USB | 5.0 − caída en pistas 5VUSB (0.2–0.3 mm) − ~0.4 V (D2) ≈ **4.5 V** | 🟡 |
| AMS1117 (dropout máx. 1.3 V a 0.8 A) | Margen ~1.35 V con batería, ~1.2 V solo con USB | 🟡 P7: solo USB y Wi-Fi a plena potencia es el caso justo |
| Disipación del AMS1117 | (4.65 − 3.3) × 0.3 A ≈ 0.4 W | ✅ SOT-223 |
| +3V3 | Plano en In2.Cu (15 µm) + pistas | ✅ |
| D4 SMBJ5.0A en 5VUSB | TVS | ✅ |

## Motores: VM → ACS725 → DRV8874

| Parámetro | Valor | Veredicto |
|---|---|---|
| ACS725 (U12) | 10 A, zonas VM/VMout con conexión sólida a IP+/IP− | ✅ |
| ITRIP de cada DRV8874 | VREF 3.3 V / (R9 o R14 = 2.4 kΩ × 450 µA/A) ≈ **3.06 A** (IMODE a GND: off-time fijo) | — |
| Alimentación VM de U5 | U5.11 → pista 0.45 mm × ~7 mm → C9 → zona | 🟠 P3: ~1.8 A para ΔT = 20 °C (IPC-2221); a 3 A, ~60 °C |
| Alimentación VM de U6 | U6.11 → pista 0.40 mm × ~7 mm → C12 → zona | 🟠 P3 |
| OUT1 (U5, U6) | 1.5 mm | ✅ |
| OUT2 U5 | 0.45 mm × ~10 mm | 🟠 P4 |
| OUT2 U6 | 0.35 mm × 3.6 mm, luego 1.5 mm | 🟠 P4 (~1.5 A para ΔT = 20 °C) |
| Desacople VM | C9/C12 0.1 µF a 6.5 mm, bulk 100 µF a 9–10 mm | ✅ (voltaje nominal de C9/C12/bulk: ver P1) |
| Bomba de carga | C8/C10 22 nF CPH–CPL (deben soportar VM), C7/C11 220 nF VCP–VM | 🟡 verificar voltaje nominal de C8/C10 |
| nSLEEP | IO14/IO11 con pull-up de 10 k → driver despierto al encender | ✅ |

## Protecciones de entrada

| Entrada | Polaridad inversa | Fusible | Picos al conectar |
|---|---|---|---|
| J19 VBUS | XT60 con forma (no se puede invertir) | ❌ | ❌ solo cerámicos (P2) |
| J20 VM | XT60 con forma | ❌ | 🟡 2 × 100 µF de bulk (amortiguan) |
| J1 USB | — | ❌ | ✅ D4 TVS |

## DRC

Sin errores de potencia. 4 errores de `hole_clearance` en el footprint del USB-C (J1, entre sus propios pads y agujeros NPTH, típico de esa pieza), advertencias de serigrafía y una pista colgante de 3.85 mm en `5V_SERVOS` (B.Cu, inofensiva). 0 ítems sin conectar.

---

## Resultado de la prueba de VBUS (10 de noviembre de 2026)

| Prueba | Resultado |
|---|---|
| VBUS de 8 a 22 V con fuente de laboratorio | ✅ Estable en todo el rango |
| 8 V con límite de corriente de la fuente ≥ 300 mA (incluye Wi-Fi en modo AP) | ✅ Estable |
| 8 V con límite de corriente < 300 mA | ❌ Brownouts repetidos: los picos de transmisión Wi-Fi superan ese límite. **Usar ≥ 300 mA de límite a 8 V** (a mayor VBUS la corriente de entrada baja proporcionalmente) |

Estado de los hallazgos tras la prueba:

| # | Estado |
|---|---|
| P1 | 🟡 La placa funcionó hasta 22 V, pero el voltaje nominal de Cin1, Chf1, C15, C9, C12, C8, C10 y los bulk **sigue sin confirmarse**. Confirmar antes de 6S (25.2 V) |
| P2 | 🟡 Abierto: no probar en caliente con baterías de 4S o más sin bulk en J19 |
| P3, P4 | Abierto: pendiente de la prueba de motores con carga |
| P5 | Abierto: pendiente de la prueba de servos con carga |
| P6, P7, P8 | Abierto |

### P9 — 5VUSB mide ~2 V alimentando solo por VBUS

**Observación (10/11/2026):** con la placa alimentada por J19 y **sin USB conectado**, el pad de 5VUSB en D4 mide ~2 V con el multímetro.

**Qué hay en la red `/5VUSB`:** solo J1 (VBUS A4/A9/B4/B9), el ánodo de D2 (SS54) y el cátodo de D4 (SMBJ5.0A) a GND. Ningún otro camino. Orientación en el PCB correcta: D2 pad 1 (K) = `/5V`, pad 2 (A) = `/5VUSB`; D4 pad 1 (K) = `/5VUSB`, pad 2 (A) = GND.

**Explicación:** con VBUS, `/5V` ≈ 4.65 V queda en el cátodo de D2 y D2 está en inversa. Un Schottky tiene corriente de fuga inversa apreciable (del orden de µA a temperatura ambiente, y crece mucho con la temperatura). Con el USB desconectado, 5VUSB queda flotando entre esa fuga de D2 (hacia `/5V`) y la fuga de D4 (hacia GND), y el multímetro (~10 MΩ) lee el punto medio. Es una fuente de **µA**, no un voltaje con capacidad de entregar corriente. Si D2 estuviera montado al revés, 5VUSB mediría ~4.2–4.3 V de forma firme, no ~2 V.

**Consecuencia al conectar un PC:**
- PC encendido: su VBUS (5 V nominales) es mayor que `/5V` desde el buck menos la caída del diodo; D2 conduce o queda casi sin polarizar. No hay corriente hacia el PC más allá de la fuga.
- PC apagado o cable conectado a un host sin alimentación: la fuga de µA no puede levantar el VBUS del host (cargado con sus condensadores y circuitos): queda prácticamente en 0 V.
- D3 impide que el USB alimente `5V_SERVOS` (los servos nunca toman corriente del PC).

**Verificación propuesta:**
1. Con VBUS aplicado y USB desconectado, poner una resistencia de 10 kΩ de 5VUSB (D4) a GND: el voltaje debe caer a unos pocos mV.
2. O medir en modo amperímetro de 5VUSB a GND: debe dar µA.

**Estado:** ✅ **Verificado inofensivo (10/11/2026):** con 10 kΩ de 5VUSB a GND el voltaje cae a ~0 mV. Es solo fuga; usar VBUS y USB a la vez es seguro. **Rev B (opcional):** resistencia de 100 kΩ de 5VUSB a GND para que el pad no flote, o un MOSFET de carga ideal en lugar de D2.

---

## Plan de prueba de potencia recomendado

0. **Antes de aplicar más de ~10 V:** confirmar el voltaje nominal de Cin1, Chf1, C15 (VBUS), C9, C12, C8, C10 y los bulk C10_Bulk1/2 (VM). Para 6S: ≥ 35 V, mejor 50 V.
1. **Fuente de laboratorio en J19, límite de 200–300 mA, sin USB, sin cargas:**
   - Subir a 8 V: el buck debe arrancar cerca de **6.97 V**. Medir 5V_SERVOS ≈ 5.07 V, /5V ≈ 4.6–4.7 V, +3V3.
   - Bajar despacio: debe parar cerca de **6.37 V**.
   - Subir por pasos hasta el voltaje máximo de la batería a usar (≤ 25.2 V), revisando la temperatura de U8.
2. **Batería:** con 4S o más, **no enchufar en caliente** directamente a J19 mientras solo haya cerámicos. Usar la fuente, un interruptor con antichispa, o poner un electrolítico de ≥ 35 V y 47–220 µF en paralelo con J19 para la prueba.
3. **Carga en 5V_SERVOS:** resistencia o carga electrónica de 0.5 → 1 → 2 → 3 A. Vigilar U8, D_2, L1 y la telemetría (reinicios y brownouts).
4. **VM:** fuente con límite de corriente, motores sin carga primero. Tocar o medir con cámara térmica las pistas VM y OUT2 de U5/U6 a 1, 2 y 3 A.
5. Durante todo: `orca_monitor.py` corriendo ([08_telemetria_wifi.md](08_telemetria_wifi.md)) y **sin USB** cuando se quiera probar el caso real (con USB, D2 sostiene /5V y oculta los brownouts).
