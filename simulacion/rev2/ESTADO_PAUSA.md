# Estado de la simulación Rev. 2 — PAUSADO (2026-11-16)

Pausado por límite de uso. Para retomar: leer este archivo y `README_rev2.md`.

## Modo de trabajo acordado con Luis
- El hilo principal solo orquesta. Simulaciones y archivos: subagentes **Sonnet**. **Fable** solo revisa resultados (solo lectura, sin simular).
- Mostrar S1–S3 a Luis y esperar su OK antes de S4b–e.
- Informes de subagentes cortos (≤ 40–50 líneas) para ahorrar tokens.
- NUNCA matar `LTspice.exe` por nombre (cortaba corridas de otros workers); solo el PID propio. Todo `.log` válido termina en "Total elapsed time".

## Decisiones de Luis
- Correcciones del `.asc` aprobadas (C_SS, C_OUT 440 µF/12 mΩ, L1 4.8 mΩ, Schottky 1N5819, sin plotwinsize, `.options solver=alt method=modtrap`).
- Q1–Q4 = **BUK9Y3R5-40E** (marcado 93E540 confirmado). Modelo en `modelos/BUK9Y3R5-40E.lib` (aportado por Luis).
- Q5/Q6 se pueden comprar fuera de JLCPCB (DigiKey/Mouser) → **Vishay SiRS4401DP**. Q7 = Vishay 2N7002K (C81445). F1 = Littelfuse 0456025.ER (C315877).

## Estado por banco

| Banco | Estado | Dónde |
|---|---|---|
| Fase A (lib xorfix, batch, diferencias .asc) | ✅ Hecho | `README_rev2.md`, `xnor_test*.log` |
| S1 polaridad | ✅ Hecho: resumen con zener `Cjo=1p`, revisado por Opus, observaciones corregidas | `S1_polaridad/resumen.md` |
| S2 hot-plug | ✅ Hecho: corregido con `Cjo=1p` y precarga de 1.0 V en los remedios; revisión de Opus (texto) aplicada | `S2_hotplug/resumen.md` |
| S3 salida | ✅ Hecho + revisado (aprobado con observaciones); S3f hecho (§10) con su revisión. Los `.log` de S3 usan `Cjo=150p` (pendiente de verificar con 1p) | `S3_salida/resumen.md` |
| S4a línea base | ✅ Hecho + revisado (aprobado con observaciones) | `S4_convertidor/resumen_S4a.md` |
| S4b–e | ⏳ En curso, worker lanzado | `S4_convertidor/` |
| S5 escalón | ✅ Hecho; criterio propuesto, **sin aprobar** | `S5_escalon/resumen.md` |
| RESULTADOS | 📝 Borrador | `RESULTADOS_Sim_Rev2_borrador.md` |
| CAMBIOS (mejoras Rev. 2) | 📝 Borrador | `CAMBIOS_Mejoras_Rev2_borrador.md` |

Decisiones de modelo vigentes: zener `BZX84C12L` con `Cjo=1p` en todo el proyecto (la librería trae 150p); límite V_DS 32 V, AL LIMITE si V_DS > 25.6 V (margen < 20 %); D1–D3 = Schottky 1N5819 (BOM); red de gate de Q6 para la hoja de decisiones = diodo en serie + `Rrst` 330 kΩ + R_G 22 kΩ, con retardo de recierre ≥ 200 ms.

## Puntos sin resolver (decisión de Luis o requieren simular)
- C_IN: 30 mΩ en S4a/S5 frente a 31 mΩ (S3 §7 usa 31 mΩ); sin unificar.
- SOA de Q5/Q6: S2 la declara no leída; S3 sí la digitalizó (curvas ±10 %).
- Criterio de S5: propuesto, sin aprobar.
- Zener de S3: `.log` con `Cjo=150p` frente al `Cjo=1p` del proyecto, y la regla de SOA u ≤ 0.7 aplicada a S3 (pendiente de verificar).
- Límite de C_carga a 25 V (100–150 µF; 150 µF AL LIMITE) y montar o no el diodo de rueda libre: decisión de Luis.

## Resultados clave hasta ahora

**S4a (12→12 V, 8 A, C_SS 12 nF, BUK9Y3R5-40E):** vout 11.992 V, fb 0.7995 V, I(L1) prom/pico/valle 9.06/9.90/8.24 A, il_max arranque 12.88 A (por 440 µF + SS acelerado), VCC 7.50 V. PGOOD del modelo TI responde: sube a 7.5 V a 1.845 ms (fin del soft-start). ~41 min de cómputo.

**S3:**
- Circuito original de Q6 (R_G 10 k) **FALLA S3a**: caída 5.6–20.7 % (9.0–20.5 % con BWK=1; antes decía "9–20 %", que era solo BWK=1; confirmado en `S3a_base.log` `.meas drop`), PGOOD cae 9/9 (BWK=1), reconmutación en 4/9 (12 y 25 V con 470/1000 µF).
- Remedio C_gd 100 nF + R_G 22 k: pasa 9/9 (caída ≤ 2.2 %; 4.4 % con fuente 3× más lenta). Pero a 25 V: apagado con 8 A > 100 ms y > 2.9 J (SOA u = 1.88) y encendido contra ≥ 330 µF fuera de SOA.
- S3b V_DS máx 30.7 V (25 V/1 mH, al límite); diodo de rueda libre opcional (footprint DNP). S3c pasa (P(Q6) ≤ 0.17 W; V_GS mín −5.5 V a 6.8 V con remedio). S3d bleeder 25→5 V en 1.07 s, 0.42 W. S3e C_IN 12→5 V en 0.58 s.
- Resistencias de LED de 4.7 k disipan ~0.12 W a 25 V → 0805 o ≥ 6.8 k.

## Observaciones de Fable a resolver

S3:
1. El convertidor idealizado es optimista (Rd ‖ Ls da corriente instantánea; ILIM 15 A > ~10 A reales).
2. Encendido a 25 V con C ≥ 330 µF: ninguna red de gate lo arregla → precarga, limitar C_carga o cambiar de FET. Límite de C_carga con ±30–40 % de incertidumbre.
3. Corregir `resumen.md` §8.3: el seguidor de apagado sería NPN (no PNP); "no abrir con 8 A a 25 V" no es exigible (PGOOD también abre Q6).

S4a: aplicar los eventos de S4c/d más tarde (vout aún asentándose a 2.4 ms); rizado con mediana de ~50 ns; definir ISNS igual en S4a–e; `.meas` de valle posterior al arranque.

## Próximos pasos (en orden)
1. Esperar el worker de S4b–e (en curso) y revisarlo.
2. Cerrar los "Puntos sin resolver" (arriba) con Luis.
3. Pasar `RESULTADOS_Sim_Rev2_borrador.md` y `CAMBIOS_Mejoras_Rev2_borrador.md` a versión final → revisión → mostrar a Luis.
