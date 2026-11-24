# Codificación de resultados — pruebas de carga

## Archivos (carpeta `logs/`)
`AAAAMMDD_HHMMSS_<comando>.<ext>` — fecha y hora de inicio de la corrida y comando que la generó (`cc`, `steps`, `dyn`, `plan`).

| Extensión | Contenido |
|---|---|
| `.csv` | Datos crudos, una fila por muestra (20 Hz) |
| `.md` | Tabla resumen por etapa (se genera al terminar, también si aborta) |
| `.png` | Gráfica Vout, Vin, Iout/Iin y Pout contra el tiempo |

Cada corrida recibe además un **ID `R##`** en [registro.md](registro.md), con condiciones y veredicto. Un CSV sin entrada en el registro no cuenta como resultado.

## Columnas del CSV
| Columna | Unidad | Origen / significado |
|---|---|---|
| `t` | s | Tiempo desde el inicio de la corrida |
| `etapa` | — | Código de etapa (tabla siguiente) |
| `iset` | A | Corriente pedida a la DL3021 (ya recortada por el tope de potencia y `--imax`) |
| `v` | V | Vout medido **en los terminales de la carga** (incluye caída de cable) |
| `i` | A | Iout medida por la DL3021 |
| `p` | W | Pout = `v`·`i` |
| `vin` | V | Vin medido en los terminales de la fuente EA-PS |
| `iin` | A | Iin medida por la fuente (resolución 0.1 A) |

## Códigos de etapa (`etapa`)
Formato general: `<letra>_<tipo>_<valor>`. La letra mantiene el orden de la secuencia `plan`.

| Código | Etapa | Condición |
|---|---|---|
| `A_sin_carga` | A — vacío | 20 s, carga OFF; referencia de V0 |
| `B_esc_<I>A` | B — escalón estático CC | `<I>` = corriente en A (10/25/50/75/100 % de Iout máx.), 40 s por escalón |
| `C_din_<lo>-<hi>A` | C — dinámico | Alterna `<lo>` y `<hi>` cada 0.5 s |
| `E_cambio_<V>V` | E — cambio de Vin | Fuente cambia a `<V>` V, sin carga (4 s) |
| `E_linea_<V>V` | E — línea | 30 % de Iout máx. con Vin = `<V>` V, 60 s |
| `E_cambio_nominal` | E — regreso a Vin nominal | Antes del soak |
| `D_soak_100%` | D — soak térmico | Iout máx. continua (480 s por defecto) |
| `reposo` | — | Carga a 0 A, 3–10 s tras una etapa (recuperación) |
| `cc_<I>A`, `esc_<I>A`, `din_<lo>-<hi>A` | — | Equivalentes de B/C para los comandos sueltos |

## Cómo se calculan los resúmenes (`.md`)
- Valores medios: segunda mitad de las muestras de la etapa (estado casi estable). Min/Max: toda la etapa.
- Eficiencia = media(Pout) / media(Vin·Iin). Solo se calcula con Pout > 1 W. A baja carga es poco fiable porque Iin tiene resolución de 0.1 A.
- Regulación de carga = (Vout medio del primer escalón B − Vout medio del último) / Vout medio del primero.

## Condiciones que cada entrada del registro debe indicar
Comando exacto, Vin, `--imax`, V0 (vacío), temperatura ambiente, fotos térmicas (nombre de archivo) y observaciones (abortos, anomalías).
