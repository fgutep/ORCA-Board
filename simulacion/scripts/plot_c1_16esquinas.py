"""
Grafica Buckboost_putavida.txt (export ASCII de LTspice, ~11 GB, doble .step
Vin_dc x Vout_target = 16 esquinas) sin cargarlo entero en memoria.

Una sola pasada por el archivo:
  - detecta cada paso por la linea "Step Information: Vin_dc=.. Vout_target=.. (Step: n/16)"
  - por paso: min/max REALES de I(L1) y V(vout) (sobre todas las muestras)
  - por paso: buffer decimado por tiempo (~PTS_PLOT puntos) para dibujar

Reusa los limites de diseno y la intencion de analizar_buckboost.py:
  IL_PEAK_MAX = 15 A, IL_CONT_MAX = 8 A.

Uso:
    python plot_putavida.py                 # usa TXT_PATH de abajo
    python plot_putavida.py otro_export.txt
    python plot_putavida.py --selftest      # prueba el parser, no lee el TXT
"""
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SIM = Path(__file__).resolve().parent.parent   # carpeta simulacion/
TXT_PATH = Path(
    sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else
    SIM / "exports" / "c1_16esquinas_2026-08-28.txt"
)
OUT_CSV = SIM / "resultados" / "c1_16esquinas_resumen.csv"
OUT_PNG = SIM / "resultados" / "c1_16esquinas_resumen.png"

IL_PEAK_MAX = 15.0   # A, pico transitorio admisible
IL_CONT_MAX = 8.0    # A, continuo nominal (4x DRV8874)
T_LOAD_STEP = 0.5e-3 # s, instante del escalon de carga (de los .meas del .log)

PTS_PLOT = 6000      # puntos por paso en la grafica (decimado por tiempo)


def parse_step_header(line):
    """'Step Information: Vin_dc=6 Vout_target=7.2  (Step: 1/16)' -> (1, 6.0, 7.2)"""
    toks = line.split()
    vin = vout = None
    idx = None
    for t in toks:
        if t.startswith("Vin_dc="):
            vin = float(t.split("=", 1)[1])
        elif t.startswith("Vout_target="):
            vout = float(t.split("=", 1)[1])
        elif "/" in t and t.rstrip(")")[0].isdigit():
            idx = int(t.rstrip(")").split("/")[0])
    return idx, vin, vout


def stream(path):
    """Genera (info_dict, t_array, vout_array, il_array) por paso."""
    with open(path, "r", buffering=1 << 20) as f:
        header = f.readline().rstrip("\n").split("\t")
        c_t = header.index("time")
        c_vo = header.index("V(vout)")
        c_il = header.index("I(L1)")

        cur = None
        for line in f:
            if line.startswith("Step Information"):
                if cur is not None:
                    yield _finish(cur)
                idx, vin, vout = parse_step_header(line)
                cur = dict(step=idx, vin_dc=vin, vout_target=vout,
                           il_max=-np.inf, il_min=np.inf,
                           vo_max=-np.inf, vo_min=np.inf,
                           vo_final=np.nan, vo_under=np.inf,
                           tp=[], vop=[], ilp=[], next_t=0.0, t_end=0.0)
                continue
            if cur is None:
                continue
            p = line.split("\t")
            try:
                t = abs(float(p[c_t])); vo = float(p[c_vo]); il = float(p[c_il])
            except (ValueError, IndexError):
                continue
            if il > cur["il_max"]: cur["il_max"] = il
            if il < cur["il_min"]: cur["il_min"] = il
            if vo > cur["vo_max"]: cur["vo_max"] = vo
            if vo < cur["vo_min"]: cur["vo_min"] = vo
            if t >= T_LOAD_STEP and vo < cur["vo_under"]:
                cur["vo_under"] = vo
            cur["vo_final"] = vo
            cur["t_end"] = t
            if t >= cur["next_t"]:
                cur["tp"].append(t); cur["vop"].append(vo); cur["ilp"].append(il)
                # se ajusta cuando conocemos t_end; arrancamos fino
                cur["next_t"] = t + 0.5e-6
        if cur is not None:
            yield _finish(cur)


def _finish(cur):
    info = {k: cur[k] for k in ("step", "vin_dc", "vout_target", "il_max",
                                "il_min", "vo_max", "vo_min", "vo_final", "vo_under")}
    if not np.isfinite(info["vo_under"]):
        info["vo_under"] = np.nan
    return (info, np.array(cur["tp"]), np.array(cur["vop"]), np.array(cur["ilp"]))


def main():
    print(f"Leyendo (streaming): {TXT_PATH}")
    if not TXT_PATH.exists():
        sys.exit(f"No existe: {TXT_PATH}")

    rows, waves = [], []
    for info, t, vo, il in stream(TXT_PATH):
        info["caso"] = f"VIN={info['vin_dc']:.1f}V -> VOUT={info['vout_target']:.1f}V"
        info["margen_OCP_A"] = IL_PEAK_MAX - info["il_max"]
        info["il_peak_OK"] = info["il_max"] <= IL_PEAK_MAX
        info["vout_err_%"] = 100.0 * (info["vo_final"] - info["vout_target"]) / info["vout_target"]
        rows.append(info)
        waves.append((info["caso"], t, vo, il))
        print(f"  paso {info['step']:2d}  {info['caso']:<28}  "
              f"il_max={info['il_max']:7.2f}A  vo_final={info['vo_final']:7.2f}V  "
              f"({len(t)} pts)")

    df = pd.DataFrame(rows).sort_values("step")
    pd.set_option("display.width", 200)
    pd.set_option("display.max_columns", None)
    cols = ["step", "caso", "il_max", "il_min", "margen_OCP_A", "il_peak_OK",
            "vo_final", "vout_err_%", "vo_under", "vo_max"]
    print("\n=== Resumen por esquina ===")
    print(df[cols].to_string(index=False))
    df.to_csv(OUT_CSV, index=False)
    print(f"\nTabla -> {OUT_CSV}")

    peligro = df[~df["il_peak_OK"]]
    if not peligro.empty:
        print(f"\nEsquinas que superan I_L_peak = {IL_PEAK_MAX} A:")
        print(peligro[["caso", "il_max"]].to_string(index=False))

    # --- grafica: V(vout) e I(L1) de las 16 esquinas superpuestas ---
    fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)
    cmap = plt.cm.turbo(np.linspace(0, 1, len(waves)))
    for (caso, t, vo, il), col in zip(waves, cmap):
        if t.size:
            axes[0].plot(t * 1e3, vo, lw=0.8, color=col, label=caso)
            axes[1].plot(t * 1e3, il, lw=0.8, color=col, label=caso)
    axes[0].set_ylabel("V_OUT [V]")
    axes[0].set_title("V_OUT en las 16 esquinas (Buckboost_putavida)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(fontsize=6, ncol=2, loc="lower right")
    axes[1].axhline(IL_PEAK_MAX, color="red", ls="--", lw=1, label=f"{IL_PEAK_MAX} A pico")
    axes[1].axhline(IL_CONT_MAX, color="orange", ls=":", lw=1, label=f"{IL_CONT_MAX} A continuo")
    axes[1].set_ylabel("I_L1 [A]")
    axes[1].set_xlabel("Tiempo [ms]")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(fontsize=6, ncol=2, loc="upper right")
    fig.tight_layout()
    fig.savefig(OUT_PNG, dpi=150)
    print(f"Grafica -> {OUT_PNG}")


def _selftest():
    sample = (
        "time\tV(vout)\tI(L1)\n"
        "Step Information: Vin_dc=6 Vout_target=7.2  (Step: 1/16)\n"
        "0.0\t0.0\t0.0\n"
        "1e-3\t7.0\t12.0\n"
        "2e-3\t7.2\t3.0\n"
        "Step Information: Vin_dc=12 Vout_target=12  (Step: 2/16)\n"
        "0.0\t0.0\t0.0\n"
        "1e-3\t11.0\t20.0\n"
        "2e-3\t12.0\t-5.0\n"
    )
    p = Path(__file__).with_name("_selftest_putavida.txt")
    p.write_text(sample)
    try:
        res = list(stream(p))
    finally:
        p.unlink()
    assert len(res) == 2, res
    i0 = res[0][0]
    assert i0["step"] == 1 and i0["vin_dc"] == 6.0 and i0["vout_target"] == 7.2
    assert i0["il_max"] == 12.0 and i0["il_min"] == 0.0
    assert i0["vo_final"] == 7.2
    assert i0["vo_under"] == 7.0   # min de V(vout) con t >= 0.5ms (fila t=1e-3)
    i1 = res[1][0]
    assert i1["il_max"] == 20.0 and i1["il_min"] == -5.0
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
    else:
        main()
