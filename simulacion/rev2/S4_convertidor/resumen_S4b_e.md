# S4b-e - Convertidor completo con entrada Q5 y salida Q6 (LTspice 26.0.2)

ESTADO: en curso (este archivo se actualiza al terminar cada corrida). Todo numero sale de un `.meas` de los `.log` de esta carpeta (se leen con `meas.py`, que ademas valida el log: un solo "Total elapsed time" y sin "tolerance relaxed"); lo demas va rotulado "(estimacion)".

## Montaje (comun a todos los casos)
- Base `S4a.cir` (LM5175 + Q1-Q4 BUK9Y3R5-40E + L1 4.7 uH + C_OUT 440 uF/12 mohm + 2x10 uF + 100 nF + C_IN, C_SS = 12 nF, ISNS igual: `N010 N010`). Todos los `.cir` se generan con `gen_S4.py` desde una sola plantilla; `.tran 0 {TEND} 0 100n`, `.options solver=alt method=modtrap`.
- Entrada: bateria (PWL a VIN_dc) -> R_bat 20 mohm -> L_cable 0.5 uH -> F1 1.92 mohm -> Q5 SiRS4401DP (drain a bateria, source a VIN; 47 kohm gate-GND; zener 12 V gate-source, catodo en source).
- Salida: Q6 SiRS4401DP (source en VOUT), 100 kohm G-S + zener 12 V G-S, R_G hacia el drenaje de Q7 (2N7002K, gate = PGOOD) a traves del interruptor de usuario `S1` (cierra en TCLOSE); bleeder 1.5 kohm en VOUT; PGOOD con pull-up 100 kohm a VCC. Carga: 8 A resistiva (R = Vout/8) + 470 uF (ESR 10 mohm, supuesto) en VEXT (detras de Q6); en S4b la carga de 8 A esta en VOUT como en S4a (Q6 abierto, `S1` nunca cierra).
- Remedio de Q6 (decision del orquestador): C_gd 100 nF en serie con diodo 1N4148W (anodo en el nodo intermedio, catodo en la gate), `Rrst` 330 kohm del nodo intermedio a source (VOUT), R_G 22 kohm. "Sin remedio" = R_G 10 kohm (baseline del circuito original), sin C_gd/diodo/Rrst.
- LED1/LED2/LED3 NO se incluyeron (no estan en el enunciado de S4; consumen ~2 mA cada uno a 12 V).
- Medidas de ripple/caida/sobreimpulso: sobre `V(vmed)` = mediana de 3 muestras (V(t), V(t-25 ns), V(t-50 ns)), ventana ~50 ns, hecha con `delay()` en una fuente B del propio netlist; la referencia es el valor previo al evento (promedio de 100-200 us antes), no el nominal. Valle de I(L1): minimo deslizante de 7 us (28 tomas a 0.25 us) en `V(vyl)`; se mide el MAXIMO de ese valle (resolucion ~0.25 us x di/dt, estimacion <~ 0.3 A).

## Desviaciones y problemas de convergencia (documentados, no ajustados en silencio)
1. Zener (DZ5 y DZ6): `BZX84C12L` de `standard.dio` con `Cjo = 1p` (la libreria trae 150p), igual que S1/S2, modelo `BZ12L1P` en el `.cir` (el nombre original choca con el de `standard.dio`).
2. Rampa de bateria **20 us** (README: 200 us). Con Q5 en el camino, rampas >= 100 us dejan la gate de Q5 "pegada" a VIN (V_GS ~ -0.1 V, Q5 apagado, VIN ~ 11.1 V, las 8 A pasan por el diodo de cuerpo) y con 200 us el solver se atasca a 0.2227 ms. Prueba aislada (Q5 + R_bat + L_cable + F1 + C_IN, sin LM5175, 4 ms): con rampa de 5 us y 20 us V(g5) -> 1.2 V (Q5 encendido); con 100 us, 1 ms y 2 ms V(g5) = 11.2 V (apagado). Con 20 us el pico de VIN en el arranque es ~13.8 V (ver S2 para el pico de enganche en caliente).
3. Con Cjo = 0 en el zener de Q5 el solver se atasca en la transicion diodo-de-cuerpo -> canal de Q5 (1.2 ms en S4b, 2.9 ms en S4c/d); con Cjo = 1p no.
4. Corridas descartadas (todas por Q5 apagado o por no convergencia): ver el historial en `gen_S4.py`; ninguna cifra de este resumen sale de ellas.

## S4b - con Q5 + R_bat + L_cable, bleeder y PGOOD; Q6 abierto (`S4b_zA.cir` / `S4b_zA.log`; identico a `S4b.cir` salvo una linea `.model` sin usar)

Corrida valida (2323 s; 0 avisos "relaxed"). Referencia: S4a (`S4a.log`, ventana 2.4-2.6 ms, V_IN = 12 V ideal).

| Medida (`.meas`) | S4a | S4b | Dif. |
|---|---|---|---|
| il_avg [A] | 9.062 | 9.258 | +2.16 % |
| il_pk [A] | 9.902 | 10.213 | +3.14 % |
| il_vy (min I(L1)) [A] | 8.245 | 8.365 | +1.46 % |
| vout_reg [V] | 11.9923 | 11.9951 | +0.02 % |
| fb_reg [V] | 0.79949 | 0.79967 | +0.02 % |
| vout_pp raw [mV] | 117.2 | 123.2 | +5.1 % |
| vout_pp mediana 50 ns [mV] | - | 122.1 | - |
| il_max (toda la corrida) [A] | 12.88 | 13.41 | +4.2 % |
| t_pgood [ms] | 1.845 | 1.790 | -3.0 % |
| t_vout11 [ms] | 1.878 | 1.818 | -3.2 % |
| VIN en regimen [V] | 12.0 (ideal) | 11.781 | -1.8 % |
| V_GS Q5 / V_DS Q5 en regimen | - | -9.30 V / 15.5 mV | R_DS(on) equiv. = 15.5 mV / 8.88 A = 1.75 mohm (estimacion, aritmetica sobre `.meas`) |
| I bateria prom. [A] | - | 8.883 | - |

Veredicto S4b: **FALLA el criterio de < 2 % en il_avg (+2.16 %) e il_pk (+3.14 %)**; il_vy (+1.46 %), vout_reg y fb_reg pasan. PGOOD sube antes de que V(vout) llegue a 11 V, al final del soft-start (PASA). Causa de la diferencia (estimacion): la caida de VIN por R_bat 20 mohm + F1 + Q5 (0.22 V, -1.8 %) sube la corriente de entrada ~ 12/11.78 = +1.9 %, cercano al +2.16 % medido; el efecto es fisico (la entrada ya no es una fuente ideal), no un artefacto de Q5. Variacion entre ventanas de S4a: il_avg 9.062 / 9.041 / 9.077 A (+-0.2 %), asi que 2.16 % no es ruido de ventana. En S4b la ventana 2.8-3.0 ms muestra vout_reg_c = 12.029 V (fb 0.80196 V) contra 12.007 V en S4a (asentamiento mas lento, no investigado).

## Precarga en S4c-e (desviacion)
Con VOUT sin carga (solo bleeder 1.5 kohm y divisor FB) el LM5175 queda en reposo (I(L1) = 0) con V(vout) = 12.24 V y a ~2.9 ms arranca una rafaga con I(L1) = -8.5 A (corriente inversa hacia la bateria) en la que el solver se atasca ("tolerance relaxed"), antes de cualquier evento. Por eso en S4c-e se deja una **precarga de 1 A en VOUT** (R = V_OUT/1 A) durante todo el caso; las 8 A + 470 uF se conectan solo en VEXT, detras de Q6. El "valor previo al evento" es el del convertidor con esa precarga (I(L1) prom. 1.41 A en el caso sin remedio).

## S4c sin remedio (R_G 10 kohm, sin C_gd) - `S4c_sin.cir` / `S4c_sin.log` (6478 s, log valido, 0 avisos)
Q6 cierra en 3 ms contra 8 A + 470 uF descargados, V_IN = 12 V, V_OUT = 12 V. Valor previo: V(vout) = 12.048 V (promedio 2.9-3.0 ms; rizado p-p con mediana 83 mV).

| Medida (`.meas`) | Valor | Criterio | Estado |
|---|---|---|---|
| caida de V(vout) (mediana 50 ns), min = 9.759 V | 18.996 % | < 9 % (ideal < 5 %) | FALLA |
| PGOOD cae | si: 3.0494 ms y de nuevo 3.4675 ms (min V(pgood) 7.4 mV) | no debe caer | FALLA |
| Q6 reabre (V_GS cruza -2.5 V hacia arriba) | no | sin rearme | PASA (Q6 no reabre; PGOOD si cae) |
| valle maximo de I(L1) (min deslizante 7 us) | 31.88 A (antes del cierre 1.24 A) | < 10.6 A | FALLA |
| I(L1) pico | 33.77 A (L1 declarada Ipk = 35 A) | - | informativo |
| I(Q6) pico | 59.71 A | - | informativo |
| Q6 encendido (V_DS < 0.5 V) | 3.4446 ms (0.44 ms tras la orden) | - | informativo |
| V(vout) 2.8-3.0 ms despues, regimen | 11.999 V, V(vext) 11.985 V, V_GS Q6 -10.91 V | - | informativo |

Lectura: el convertidor real confirma el arbitro de S3a: sin remedio el cierre contra 470 uF + 8 A hunde V(vout) casi 19 % (S3a idealizada daba 9-20 %), PGOOD cae y el LM5175 llega a corriente de inductor ~34 A (saturando L1). El valle de 31.9 A es un artefacto-limite del modelo (la limitacion por ciclo del modelo TI no contiene la corriente); el numero es el medido.

## S4c / S4d / S4e con remedio
(en curso; ver el final de este archivo)
