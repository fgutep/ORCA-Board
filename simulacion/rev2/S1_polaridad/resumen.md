# S1 - Proteccion de polaridad (Q5 = SiRS4401DP), Rev. 2

Sin LM5175. LTspice 26.0.2, `.options solver=alt method=modtrap`. Los cinco `.log` se regeneraron con los `.cir` actuales (zener con `Cjo=1p`): todos terminan en `Total elapsed time` y ninguno muestra `Simulation tolerance relaxed`.

**Procedencia de los numeros.** Los valores de las tablas salen de los `.meas` de los `.log`, o de aritmetica directa sobre ellos (margenes, P = V_DS x I). Lo que no es `.meas` va rotulado **(estimacion)** donde aparece.

**Criterio de estado.** FALLA si no cumple; AL LIMITE si cumple con margen < 20 % del limite; PASA si no. Limite de V_DS unificado en **32 V** para todo el banco: el margen se calcula contra 32 V y es AL LIMITE si V_DS > 25.6 V (= 0.8 x 32 V). Los 40 V son el maximo absoluto de la pieza, no el limite de diseno.

## Circuito simulado
BAT (fuente ideal) + R_bat 20 mohm + L_cable 0.5 uH -> F1 (1.92 mohm) -> Q5 (drain a bateria, source a VBUS) -> VBUS.
Q5: 47 kohm gate-GND, zener 12 V gate-source (catodo en source). C_IN = 100 uF polimero (ESR 31 mohm) + 10 uF (3 mohm) + 100 nF (10 mohm). LED1 + 4.7 kohm de VBUS a GND.
Modelo Q5: `..\modelos\SiRS4401DP_PS.TXT` (Vishay, `.subckt SiRS4401DP d g s`), incluido por ruta relativa.

Zener `BZX84C12L` de `C:\Users\Lenovo\AppData\Local\LTspice\lib\cmp\standard.dio` **con `Cjo=1p`, igual que S2**. Es una **desviacion respecto de la libreria**, que trae `Cjo=150p`: con 150p, S2 (rampa de BAT hasta 25.2 V) no converge ("Simulation tolerance relaxed" al entrar en conduccion el diodo de Q5 con el zener y su Cjo en el gate), asi que todo el proyecto usa 1p para tener un solo modelo. El LED `QTLP690C` es la copia literal de `standard.dio`. Ninguno es el MM3Z12VT1G ni el LED real. Los cinco `.cir` usan exactamente estas dos lineas:

```
.model BZX84C12L D(Is=.6n Rs=.5 Cjo=1p nbv=5 bv=12 Ibv=1m Vpk=12 mfg=OnSemi type=Zener)
.model QTLP690C D(Is=1e-22 Rs=6 N=1.5 Cjo=50p Xti=100 Iave=160m Vpk=5 mfg=Fairchild type=LED)
```
Con este zener (rodilla suave) V_GS = -11.37 V a V_IN = 12 V y -11.84 V a 25.2 V, 25 C (`.meas vgs12`). El MM3Z12VT1G real tiene su propia tolerancia de tension, no modelada. Para S2 ver `S2_hotplug\resumen.md`: ambos bancos usan el mismo modelo (zener `BZX84C12L`, `Cjo=1p`).
ESR del polimero: KEMET A759KS107M1V E031 (100 uF / 35 V, 8x12 mm) = 31 mohm max a 100 kHz, 20 C (`Datasheets\KEMET_A4072_A759_condensadores.pdf`).

## Tabla pasa/falla

| Caso | Criterio | Medido (`.meas`) | Estado |
|---|---|---|---|
| S1a | \|V_GS\| <= 15 V | max 11.84 V (25.2 V, 25 C); 5.75 V a 6 V | PASA (margen 21 %) |
| S1a | P(Q5) <= 1 W a 12 A, todos los V_IN, 25 y 100 C | max 0.387 W (6 V, 100 C); 0.282 W (6 V, 25 C) | PASA (margen 61 %) |
| S1a | idem con R_DS(on) de datasheet (**estimacion** x1.5 sobre `p12`, no es del modelo) | 0.58 W (6 V, 100 C) | PASA (margen 42 %) |
| S1a | pulso 23 A 10 ms (sin criterio) | P 0.88-1.06 W (25 C), 1.21-1.45 W (100 C) | informativo |
| S1b | \|V(VBUS)\| < 0.5 V con BAT = -25.2 V | 0.058 mV (25 C); 0.532 mV (100 C) | PASA (margen 99.9 %) |
| S1b | fuga < 1 mA | 2.9 nA (25 C); 0.426 uA (100 C) | PASA |
| S1b | V_DS < 32 V | 25.20 V (25 y 100 C) | PASA (margen 21 %, bajo el umbral AL LIMITE de 25.6 V) |
| S1c | Q5 sigue conduciendo a 5 A | V_GS -6.12 V (6 V) .. -11.84 V (25.2 V); V_DS -8.4 .. -9.7 mV; P(Q5) 42-48 mW | PASA |
| S1c | V(VBUS) - V(BAT) < 0.2 V, regimen (BAT = terminal tras R_bat) | 0.023-0.024 V | PASA |
| S1c | idem respecto de la FEM de la bateria (incluye 5 A x 20 mohm de R_bat) | 0.123-0.124 V | PASA (margen 38 %) |
| S1c | pico durante el flanco de la corriente: 5 A en 5 us (1 A/us) | 0.240 V | FALLA (informativo, depende del slew; ver abajo) |
| S1c | pico con flanco 5 A en 100 us | 0.047-0.049 V | PASA |
| S1d | \|V_GS\| <= 20 V (maximo de la pieza) y V_DS < 32 V - caso 1 (+25.2 -> -25.2 V en 1 ms) | V_GS -11.84..+0.35 V; V_DS inverso 0.03 V; V_DS directo 4-20 mV | PASA |
| S1d | idem caso 2 (+25.2 -> 0 en 1 ms, 1 ms en 0, 0 -> -25.2 en 1 ms) | V_GS -11.84..+0.34 V; V_DS inverso 12.2 V | PASA (V_DS margen 62 % vs 32 V) |
| S1d | **Observacion**: V(VBUS) minimo (referencia: criterio de S1b, \|V(VBUS)\| < 0.5 V) | caso 1: -25.38 V; caso 2: -13.03 V (sin carga) | FALLA (informativo, ver abajo) |
| S1d (extra, no pedido) | caso 3: +25.2 -> -25.2 V en 10 us (conexion inversa abrupta con C_IN cargado) | V(VBUS) -39.7 V; I(Q5) -431 A; P pico 314 W; V_GS -12.05 V | FALLA (informativo; ver nota de I_DM) |

Nota caso 3: la corriente pico de -431 A es `.meas` (`i_min`). El valor de I_DM = 350 A / 100 us proviene del datasheet de la pieza y no se verifico aqui; el `.meas` no mide cuanto tiempo se supera.

## P(Q5) vs V_IN a 12 A (tabla pedida; R_bat 20 mohm, F1 incl.; V_GS en regimen)

| V_IN (V) | V_GS (V) | R_DS(on) 25 C (mohm) | P 25 C (W) | R_DS(on) 100 C (mohm) | P 100 C (W) | P a 23 A, 25 C / 100 C (W) |
|---|---|---|---|---|---|---|
| 6 | -5.75 | 1.96 | 0.282 | 2.68 | 0.387 | 1.060 / 1.454 |
| 7.4 | -7.15 | 1.83 | 0.263 | 2.51 | 0.361 | 0.978 / 1.342 |
| 12 | -11.37 | 1.68 | 0.242 | 2.30 | 0.331 | 0.889 / 1.219 |
| 16.8 | -11.71 | 1.67 | 0.241 | 2.29 | 0.330 | 0.885 / 1.211 |
| 25.2 | -11.84 | 1.67 | 0.241 | 2.29 | 0.329 | 0.884 / 1.210 |

(V_GS dado a 25 C; a 100 C: -5.50 V a 6 V.) El punto critico de 6 V (V_GS ~ -6 V) cumple con holgura: la pieza esta bien encendida; no hace falta otra pieza. R_DS(on) = V_DS/I medidos (`.meas rds12`, `rds23`).

## Hallazgos que conviene que lea el orquestador

1. **S1d: el protector no bloquea un barrido rapido con C_IN cargado.** Con el gate a GND por 47 kohm, la constante gate-source (47 kohm x ~9 nF ~ 0.42 ms; **estimacion**, la ~9 nF de C_gs no es un `.meas`) mantiene a Q5 encendido mientras BAT cae (50 V/ms, pendiente del estimulo); VBUS sigue a la bateria hasta -25.4 V (`vbus_min`) y C_IN queda cargado en inversa (el polimero y el pin VIN del LM5175 sufririan). Q5 en si no excede V_GS ni V_DS (criterio literal PASA). Es un escenario de conexion inversa con VBUS aun cargado (el LED + 4.7 kohm descarga con tau ~ 0.5 s, **estimacion** 4.7 kohm x ~110 uF), no de conexion inversa en frio (esa es S1b: bloquea).
2. **Remedio probado (`S1d_remedio_rg.cir`, caso 1, sin carga): bajar R_gate.** V(VBUS) minimo (`.meas vbus_min`): 47 kohm -25.4 V; 22 kohm -18.8 V; 10 kohm -10.2 V; 4.7 kohm -4.84 V; 2.2 kohm -1.42 V; 1 kohm +0.59 V. **Ningun valor practico resuelve el criterio de 0.5 V**; 1 kohm queda cerca (+0.59 V) pero con 12.9 mA en el zener en regimen a 25.2 V (`ig_dc`) y P_zener = 154 mW (**estimacion**: `ig_dc` medido x 12 V asumidos; con 2.2 kohm, 71 mW). Con 1 kohm, ademas, V_DS inverso = 25.81 V (`vds_rev`), que cae AL LIMITE (> 25.6 V sobre el limite de 32 V). Si el escenario se considera real, la solucion es otra (p. ej. descarga rapida de VBUS o diodo en serie a nivel de sistema), no un ajuste de R_gate. Decision para el orquestador.
3. **S1c, pico de 0.24 V**: es L_cable (0.5 uH) x di/dt del estimulo (1 A/us), no el regimen (atribucion por inspeccion: con 0.05 A/us el `.meas` baja a 0.047 V). El criterio de 0.2 V es de regimen; aplicado al pico es solo informativo.
4. Margen de \|V_GS\| a 25.2 V es 21 % (zener soft-knee 11.84 V vs 15 V): PASA, pero esta apenas sobre el umbral de 20 %. Con una tolerancia de +-5 % del zener real, V_GS subiria a ~12.4-12.6 V (**estimacion**, 11.84 V x 1.05 / 12 V x 1.05; la tolerancia no esta modelada), aun < 15 V.

## Supuestos y limitaciones

- **S1a, S1c y S1d arrancan desde el punto de operacion con la bateria ya conectada** (sin rampa de BAT en el arranque); con el zener `Cjo=1p` los cinco bancos convergen. El arranque con rampa de BAT no se simulo en S1. **Pendiente de verificar**: ningun `.log` de S1 documenta que la rampa lenta no converja; el unico respaldo medido es el caso 12.6 V / 30 mohm / 0.2 uH de S2 con rampa de BAT desde 0 V (`..\S2_hotplug\S2a_hotplug_12V_30m_0u2_precarga.log`, no converge, cortado a 200 s; ver `..\S2_hotplug\resumen.md`).
- No se simulo una conexion lenta (rampa de varios cientos de us); en ese caso el gate sigue al source via Cgs y Q5 tarda ~ms en encender (razonamiento fisico), sin riesgo para la pieza porque conduce por el diodo: **estimacion** ~10 W durante ~1 ms a 12 A, no simulado.
- Temperatura con `.step temp` (25 y 100 C): solo escala el modelo Vishay (tc del R_DS(on) y Vth); no hay termica acoplada ni zener/LED con tc propio.
- La **estimacion** "peor caso datasheet" (x1.5) usa 2.9 mohm max a -4.5 V (datasheet) frente a 1.96 mohm del modelo a -5.75 V (`rds12`); es aproximada.
- S1b: peor caso de alta impedancia (VBUS sin carga, solo LED1); la fuga viene del diodo del modelo (no hay I_DSS explicita; datasheet: -1 uA a 40 V, 25 C).
- S1c: BAT = fuente ideal con R_bat 20 mohm; con 30 mohm el delta vs FEM seria ~0.17 V, AL LIMITE (**estimacion** aritmetica: 0.123 V + 5 A x 10 mohm, no simulado). Fuente de corriente ideal.
- F1 = 1.92 mohm constante (sin termica). Sin ESL de capacitores ni derating por DC bias de los ceramicos.
- Los modelos zener/LED son genericos de `standard.dio` (zener con `Cjo=1p`, desviacion de la libreria), no del MM3Z12VT1G / LED real.

## Archivos
`S1a_directa.cir/.log`, `S1b_inversa.cir/.log`, `S1c_regeneracion.cir/.log`, `S1d_transitorio.cir/.log`, `S1d_remedio_rg.cir/.log`, `csv/` (S1d, casos sin carga, 5000 pts, ventana 5.9-10 ms; regenerados por `raw2csv.py` desde los `.raw` de esta corrida), `raw2csv.py`. Los `.raw` se regeneran corriendo los `.cir`.
