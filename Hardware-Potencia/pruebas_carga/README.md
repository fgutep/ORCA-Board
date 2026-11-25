# Pruebas de carga — placa de potencia LM5175 (hasta 120 W)

Carga: **Rigol DL3021** (USB). Fuente: **EA PS 10060-170** (COM12, comandos tomados de `PV-Emulator/comm/scpi.py`). Script: [carga.py](carga.py) (`pip install pyvisa pyvisa-py pyserial pandas matplotlib`). Resultados: [registro.md](registro.md) (veredicto por corrida) y [CODIGOS.md](CODIGOS.md) (codificación). Cada corrida deja en `logs/`: `.csv` (t, etapa, Iset, Vout, Iout, Pout, Vin, Iin a 20 Hz), `.md` (tabla resumen) y `.png`.

## Antes de empezar
1. Etapas 0–6 del [protocolo de bring-up](../physical_debug/02_protocolo_bringup.md) ya cumplidas; V_OUT fijado con RV1 (V_OUT ≤ 1.4 × V_IN en boost).
2. El script **enciende la fuente solo** (`--vin`), con límite de corriente = 1.25 × 120 W / (0.85 × V_IN) y protecciones OVP/OCP/OPP (las que traía eran 20 V / 2 A / 30 W). Al terminar o abortar la apaga. Cables gruesos y cortos; Vin/Iin se leen en los terminales de la fuente, no de la placa.
3. Si la placa no arranca (V_OUT < 3 V) o el consumo en vacío es > 1 A, aborta y apaga. Con V_OUT = 12 V, 120 W pide 10 A: usar `--imax 10` (por defecto el tope es 8 A, el continuo de diseño).
4. Osciloscopio (ripple / transitorios) listo. Fotos térmicas (Keysight U5855A, desde el 24-nov) en [fotos_termicas/](fotos_termicas/), nombradas `AAAAMMDD_HHMM_Rxx_<descripción>.png`.

## Comandos
```
python carga.py --vin 12 cc 2.5 --t 60                  # corriente constante
python carga.py --vin 12 steps 10% 25% 50% 75% 100% --dwell 40   # escalones (A, o % de I max = min(imax, 120 W / V_OUT))
python carga.py --vin 12 dyn 25% 75% --ton 0.5 --toff 0.5 --n 20 # escalón repetitivo
python carga.py --vin 12 --imax 10 plan --vins 9 12 16.8        # secuencia completa
python carga.py report logs/<archivo>.csv      # regenerar tabla y gráfica
```
`--slew` (A/µs, default 0.05) fija la pendiente de los escalones. `--pmax` (W, default 120, tope 200 = máximo de la DL3021) sube el tope de potencia, el límite de corriente y el de potencia de la fuente. Por encima de ~120 W la placa no aguanta en continuo: Q3/Q4 llegaron a 180 °C en ~1 min a 200 W (R22). **Ctrl-C** = entrada OFF inmediata.
Protecciones (software): P ≤ 120 W (+3 %), I ≤ `--imax`, aborta si V_OUT < 80 % del valor en vacío (límite de corriente/hiccup).
Cada comando arranca con la fuente apagada y la enciende (la placa se reinicia en cada comando; `plan` lo hace una sola vez). Eficiencia = Pout / (Vin·Iin) medidos automáticamente.
Protecciones extra de entrada: aborta si Vin < 90 % de lo fijado o Iin ≥ 98 % del límite (fuente en CC). Puntos de línea con V_OUT > 1.4 × V_IN se omiten.
Arranque: la fuente sube de 0 a `--vin` en rampa de ~2 s (10 pasos de 0.2 s) para limitar el inrush.
`plan --etapas CED` corre solo las etapas indicadas, en ese orden. Ante un apagado de la placa el plan **se detiene** (`--max-abortos 0` por defecto); subir ese valor reinicia la fuente y continúa, pero no se recomienda: re-energizar una placa con falla puede agravar el daño (ver [registro.md](registro.md), R03–R06).
Durante las pruebas de carga, medir EN (pad de R2) con el multímetro: debe quedarse en ≈ 0.25 × V_IN.

## Pruebas principales (≈ 22 min con `plan`, 30 min con montaje)

| # | Prueba | Qué se hace | Qué demuestra | Tiempo |
|---|---|---|---|---|
| 0 | Humo | `cc 0.5 --t 5` | La carga regula y los cables están bien | 1 min |
| A | Sin carga | 20 s en reposo | V_OUT en vacío, referencia | 0.3 min |
| B | Escalones estáticos (CC) | 10 / 25 / 50 / 75 / 100 % × 40 s | Regulación de carga, eficiencia vs. carga | ≈ 5 min |
| C | Transitorio dinámico | 25↔75 % y 50↔100 %, 20 ciclos de 1 s | Caída/sobreimpulso de V_OUT (capturar con osciloscopio), estabilidad del lazo | ≈ 1 min |
| E | Línea (V_IN) | 30 % de carga con V_IN = 9, 12, 16.8 V (V_OUT = 12 V: boost, transición, buck) × 60 s (la fuente cambia sola) | Modos boost / transición / buck | ≈ 4.5 min |
| D | Soak térmico | 100 % (120 W) por 8 min, avisos de inicio/mitad/final (fotos térmicas si hay cámara) | Temperatura de Q1–Q4, L1, LM5175; deriva de V_OUT | ≈ 8.5 min |

Criterio de paso sugerido: regulación < 2 %, sin hiccup ni aborto, T de componentes estable al final del soak (sin deriva ascendente), eficiencia ≥ 90 % a carga media/alta.
Orden: de menor a mayor estrés; el soak va al final.
