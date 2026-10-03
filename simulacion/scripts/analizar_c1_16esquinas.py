"""
Analiza el .raw + .log de una corrida de LTspice con doble .step
(VIN_dc x VOUT_target) y escalon de carga.

Pensado para Buckboost_fixed.asc con las directivas:
    .step param VIN_dc list 6 12 22.2 25.2
    .step param VOUT_target list 7.2 12 18 25
    .meas TRAN vout_undershoot ...
    .meas TRAN vout_recovery_time ...
    .meas TRAN il_max ...
    .meas TRAN il_min ...

No depende de la API interna de PyLTSpice para leer los parametros de
cada paso (eso varia entre versiones) -- los parametros vin_dc/vout_target
y los resultados de .meas se leen directo del .log con expresiones
regulares sobre el formato de texto que LTspice ya escribe ahi (estable
entre versiones). PyLTSpice solo se usa para sacar las formas de onda
del .raw (V(vout), I(L1), etc.) por paso.

Requiere:
    pip install PyLTSpice pandas matplotlib numpy

Uso:
    python analizar_buckboost.py "C:\\ruta\\a\\Buckboost_fixed.raw"
    (si no pasas ruta, usa la de RAW_PATH mas abajo)
"""

import re
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# Import de RawRead con fallback: PyLTSpice re-exporta RawRead en versiones
# recientes, pero el motor se movio a "spicelib" en algun momento. Probamos
# ambos caminos para no depender de cual tengas instalada.
# ---------------------------------------------------------------------------
RawRead = None
_import_errors = []
for _module, _name in [("PyLTSpice", "RawRead"), ("spicelib", "RawRead")]:
    try:
        RawRead = getattr(__import__(_module, fromlist=[_name]), _name)
        break
    except Exception as e:  # noqa: BLE001
        _import_errors.append(f"{_module}: {e}")

if RawRead is None:
    sys.exit(
        "No pude importar RawRead de PyLTSpice ni de spicelib.\n"
        "Instala con:  pip install PyLTSpice\n"
        "(si ya lo tienes, prueba: pip install --upgrade PyLTSpice spicelib)\n"
        "Detalle de los intentos:\n  " + "\n  ".join(_import_errors)
    )

# ---------------------------------------------------------------------------
# CONFIGURACION -- ajusta esto
# ---------------------------------------------------------------------------
SIM = Path(__file__).resolve().parent.parent   # carpeta simulacion/
RAW_PATH = Path(
    sys.argv[1] if len(sys.argv) > 1 else
    SIM / "Buckboost_fixed.raw"
)
LOG_PATH = RAW_PATH.with_suffix(".log")

# Limites de diseno de Carga Luis, para las columnas PASS/FAIL
IL_PEAK_MAX = 15.0   # A, transitorio/pico (2 motores en stall simultaneo + margen)
IL_CONT_MAX = 8.0    # A, continuo nominal (4x DRV8874)

OUT_CSV = SIM / "resultados" / "c1_16esquinas_casos.csv"
OUT_PNG = SIM / "resultados" / "c1_16esquinas_casos.png"

STEP_VARS = ["vin_dc", "vout_target"]  # nombres tal como aparecen en ".step param ..."


# ---------------------------------------------------------------------------
# 1. Parametros de cada paso, leidos de las lineas ".step vin_dc=.. vout_target=.."
#    que LTspice escribe en el .log, en el mismo orden en que corrio cada paso.
# ---------------------------------------------------------------------------
def read_log_text(path: Path) -> str:
    for enc in ("utf-16", "utf-8", "latin-1"):
        try:
            text = path.read_text(encoding=enc, errors="strict")
            if text.strip():
                return text
        except (UnicodeError, UnicodeDecodeError):
            continue
    # ultimo recurso: ignorar errores de decodificacion
    return path.read_text(encoding="latin-1", errors="ignore")


def parse_step_params(log_text: str, step_vars):
    pattern = re.compile(
        r"^\.step\s+(.*)$", re.MULTILINE | re.IGNORECASE
    )
    steps = []
    for m in pattern.finditer(log_text):
        line = m.group(1)
        params = dict(re.findall(r"(\w+)=([-\d.eE+]+)", line))
        row = {}
        for v in step_vars:
            key = next((k for k in params if k.lower() == v.lower()), None)
            row[v] = float(params[key]) if key is not None else None
        steps.append(row)
    return steps


def parse_log_measurements(log_text: str):
    """Lee los bloques 'Measurement: <nombre>' que LTspice escribe al final
    del .log cuando hay .step + .meas. Formato:
        Measurement: vout_undershoot
          step	vout_undershoot
             1	7.0543
             2	11.892
    """
    blocks = re.split(r"\n(?=Measurement:)", log_text)
    data = {}
    for block in blocks:
        m = re.match(r"Measurement:\s*(\S+)", block)
        if not m:
            continue
        name = m.group(1)
        rows = re.findall(r"^\s*(\d+)\s+([-\d.eE+]+)\s*$", block, re.MULTILINE)
        if not rows:
            continue
        data[name] = {int(idx): float(val) for idx, val in rows}
    return data


print(f"Leyendo log: {LOG_PATH}")
log_text = read_log_text(LOG_PATH)
step_params = parse_step_params(log_text, STEP_VARS)
meas = parse_log_measurements(log_text)

n_steps = len(step_params)
if n_steps == 0:
    sys.exit(
        "No encontre lineas '.step vin_dc=... vout_target=...' en el .log.\n"
        "Revisa que LOG_PATH apunte al .log correcto (mismo nombre que el .raw)."
    )
print(f"{n_steps} pasos encontrados en el log.")
print("Mediciones encontradas:", list(meas.keys()) if meas else "(ninguna -- revisa el .log)")

# ---------------------------------------------------------------------------
# 2. Tabla resumen: una fila por esquina (VIN_dc, VOUT_target)
# ---------------------------------------------------------------------------
rows = []
for i, params in enumerate(step_params):          # i: 0-based (python / PyLTSpice)
    ltspice_step = i + 1                           # LTspice numera los pasos desde 1
    row = {"step": ltspice_step, **params}
    for name, values in meas.items():
        row[name] = values.get(ltspice_step, np.nan)
    rows.append(row)

df = pd.DataFrame(rows)
df["caso"] = df.apply(
    lambda r: f"VIN={r.vin_dc:.1f}V -> VOUT={r.vout_target:.1f}V", axis=1
)

if "il_max" in df.columns:
    df["margen_OCP_A"] = IL_PEAK_MAX - df["il_max"]
    df["il_peak_OK"] = df["il_max"] <= IL_PEAK_MAX

if "vout_recovery_time" in df.columns:
    df["recovery_encontrado"] = df["vout_recovery_time"].notna()

pd.set_option("display.width", 160)
pd.set_option("display.max_columns", None)
print("\n=== Resumen por esquina ===")
print(df.to_string(index=False))

df.to_csv(OUT_CSV, index=False)
print(f"\nTabla guardada en: {OUT_CSV}")

if "recovery_encontrado" in df.columns:
    faltantes = df[~df["recovery_encontrado"]]
    if not faltantes.empty:
        print("\nEsquinas sin vout_recovery_time (no cerro dentro de la ventana del .tran):")
        print(faltantes[["caso"]].to_string(index=False))

if "il_peak_OK" in df.columns:
    peligrosos = df[~df["il_peak_OK"]]
    if not peligrosos.empty:
        print(f"\nEsquinas que superan I_L_max = {IL_PEAK_MAX} A:")
        print(peligrosos[["caso", "il_max"]].to_string(index=False))

# ---------------------------------------------------------------------------
# 3. Formas de onda: V(vout) e I(L1) para las 16 corridas, superpuestas
# ---------------------------------------------------------------------------
try:
    print(f"\nLeyendo raw: {RAW_PATH}")
    raw = RawRead(str(RAW_PATH))
    trace_names = raw.get_trace_names()
    print("Trazas disponibles:", trace_names)

    def get_xy(signal, step_idx):
        trace = raw.get_trace(signal)
        y = trace.get_wave(step_idx)
        try:
            x = raw.get_axis(step_idx)
        except AttributeError:
            x = raw.get_trace("time").get_wave(step_idx)
        return np.array(x), np.array(y)

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

    vout_name = next((t for t in trace_names if t.lower() == "v(vout)"), None)
    il_name = next((t for t in trace_names if t.lower() == "i(l1)"), None)

    if vout_name:
        for i in range(n_steps):
            x, y = get_xy(vout_name, i)
            axes[0].plot(x * 1e3, y, linewidth=0.9, label=df["caso"][i])
        axes[0].set_ylabel("V_OUT [V]")
        axes[0].set_title("V_OUT en las 16 esquinas (escalon de carga)")
        axes[0].grid(True, alpha=0.3)
        axes[0].legend(fontsize=6, ncol=2, loc="lower right")
    else:
        print("No encontre V(vout) en el .raw -- revisa el nombre exacto con trace_names.")

    if il_name:
        for i in range(n_steps):
            x, y = get_xy(il_name, i)
            axes[1].plot(x * 1e3, y, linewidth=0.9, label=df["caso"][i])
        axes[1].axhline(IL_PEAK_MAX, color="red", linestyle="--", linewidth=1,
                         label=f"{IL_PEAK_MAX} A limite")
        axes[1].set_ylabel("I_L1 [A]")
        axes[1].grid(True, alpha=0.3)
        axes[1].legend(fontsize=6, ncol=2, loc="upper right")
    else:
        print("No encontre I(L1) en el .raw -- revisa el nombre exacto con trace_names.")

    axes[1].set_xlabel("Tiempo [ms]")
    fig.tight_layout()
    fig.savefig(OUT_PNG, dpi=150)
    print(f"Grafica guardada en: {OUT_PNG}")
    plt.show()

except Exception as e:  # noqa: BLE001
    print(f"\nNo pude graficar las formas de onda del .raw ({e}).")
    print("La tabla de .meas (arriba y en el CSV) sigue siendo valida sin esto.")
    print("Si el error es de API, revisa con: help(RawRead) para tu version instalada.")
