"""
Grafica Buckboost_V1.txt (export ASCII de LTspice, ~9 GB) sin cargarlo entero.

Misma idea que plot_putavida.py pero para el otro setup de Buckboost_fixed.asc:
    .step param idx 1 4 1
    VIN_dc      = table(idx, 1,6,   2,12, 3,22.2, 4,25.2)
    VOUT_target = table(idx, 1,7.2, 2,12, 3,25,   4,7.2)
    Iload = PULSE(0 6.4 24m 1u 1u 4m 1)   -> escalon de carga 2A -> 8.4A en t=24ms,
                                             y de vuelta a 2A en t=28ms
    columnas: time V(comp) V(fb) V(ss) V(vin) V(vout) I(L1) I(V1)

Una sola pasada:
  - min/max REALES de I(L1), V(vout), I(V1) por paso (todas las muestras)
  - stats en ventanas (igual que los .meas del .log):
      il_*_step   : 24-28 ms  (ventana del escalon)
      vout_under  : 24-26 ms
      P_in/P_out  : 27-28 ms  ->  eficiencia
  - buffer decimado por tiempo para dibujar V(vout), I(L1), I(V1)

Uso:
    python plot_v1.py
    python plot_v1.py otro_export.txt
    python plot_v1.py --selftest
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
    SIM / "exports" / "c2_4esquinas_2026-08-29.txt"
)
OUT_CSV = SIM / "resultados" / "c2_4esquinas_resumen.csv"
OUT_PNG = SIM / "resultados" / "c2_4esquinas_resumen.png"

IL_PEAK_MAX = 15.0
IL_CONT_MAX = 8.0

# idx -> (VIN_dc, VOUT_target)  (table() de Buckboost_fixed.asc)
IDX_TABLE = {1: (6.0, 7.2), 2: (12.0, 12.0), 3: (22.2, 25.0), 4: (25.2, 7.2)}

ILOAD_BASE = 2.0
ILOAD_DELTA = 6.4          # 8.4 - 2
W_STEP = (0.024, 0.028)    # ventana del escalon (il_*_step)
W_UNDER = (0.024, 0.026)   # vout_under
W_PWR = (0.027, 0.028)     # p_in / p_out / vout_reg

DT_PLOT = 4e-6             # decimado para dibujar (~8000 pts en 32 ms)


def parse_step_header(line):
    """'Step Information: Idx=1  (Step: 1/4)' -> 1"""
    for t in line.split():
        if t.startswith("Idx="):
            return int(t.split("=", 1)[1])
    return None


def stream(path):
    with open(path, "r", buffering=1 << 20) as f:
        header = f.readline().rstrip("\n").split("\t")
        c_t = header.index("time")
        c_vo = header.index("V(vout)")
        c_il = header.index("I(L1)")
        c_vin = header.index("V(vin)")
        c_iv1 = header.index("I(V1)")

        cur = None
        for line in f:
            if line.startswith("Step Information"):
                if cur is not None:
                    yield _finish(cur)
                idx = parse_step_header(line)
                vin, vout = IDX_TABLE.get(idx, (np.nan, np.nan))
                cur = dict(idx=idx, vin_dc=vin, vout_target=vout,
                           il_max=-np.inf, il_min=np.inf,
                           vo_max=-np.inf, vo_min=np.inf,
                           iv1_max=-np.inf, iv1_min=np.inf,
                           vo_final=np.nan,
                           il_step_max=-np.inf, il_step_min=np.inf,
                           vo_under=np.inf,
                           # integrales trapezoidales (time-weighted, como AVG de LTspice)
                           pin_sum=0.0, pout_sum=0.0, iin_sum=0.0,
                           vo_reg_sum=0.0, dt_sum=0.0,
                           prev=None,
                           tp=[], vop=[], ilp=[], iv1p=[], next_t=0.0)
                continue
            if cur is None:
                continue
            p = line.split("\t")
            try:
                t = abs(float(p[c_t]))
                vo = float(p[c_vo]); il = float(p[c_il])
                vin = float(p[c_vin]); iv1 = float(p[c_iv1])
            except (ValueError, IndexError):
                continue

            if il > cur["il_max"]: cur["il_max"] = il
            if il < cur["il_min"]: cur["il_min"] = il
            if vo > cur["vo_max"]: cur["vo_max"] = vo
            if vo < cur["vo_min"]: cur["vo_min"] = vo
            if iv1 > cur["iv1_max"]: cur["iv1_max"] = iv1
            if iv1 < cur["iv1_min"]: cur["iv1_min"] = iv1
            cur["vo_final"] = vo

            if W_STEP[0] <= t <= W_STEP[1]:
                if il > cur["il_step_max"]: cur["il_step_max"] = il
                if il < cur["il_step_min"]: cur["il_step_min"] = il
            if W_UNDER[0] <= t <= W_UNDER[1] and vo < cur["vo_under"]:
                cur["vo_under"] = vo

            # AVG de LTspice = integral(f dt)/(t1-t0) -> trapecios entre muestras
            r7 = cur["vout_target"] / ILOAD_BASE
            pin_i = -vin * iv1
            pout_i = vo * (vo / r7 + ILOAD_DELTA)
            prev = cur["prev"]
            if prev is not None and W_PWR[0] <= prev[0] and t <= W_PWR[1]:
                dt = t - prev[0]
                cur["dt_sum"] += dt
                cur["pin_sum"] += 0.5 * (pin_i + prev[1]) * dt
                cur["pout_sum"] += 0.5 * (pout_i + prev[2]) * dt
                cur["iin_sum"] += 0.5 * (-iv1 + -prev[3]) * dt
                cur["vo_reg_sum"] += 0.5 * (vo + prev[4]) * dt
            cur["prev"] = (t, pin_i, pout_i, iv1, vo)

            if t >= cur["next_t"]:
                cur["tp"].append(t); cur["vop"].append(vo)
                cur["ilp"].append(il); cur["iv1p"].append(iv1)
                cur["next_t"] = t + DT_PLOT
        if cur is not None:
            yield _finish(cur)


def _finish(cur):
    d = cur["dt_sum"]
    p_in = cur["pin_sum"] / d if d else np.nan
    p_out = cur["pout_sum"] / d if d else np.nan
    info = dict(
        idx=cur["idx"], vin_dc=cur["vin_dc"], vout_target=cur["vout_target"],
        il_max=cur["il_max"], il_min=cur["il_min"],
        il_step_max=_nan(cur["il_step_max"]), il_step_min=_nan(cur["il_step_min"]),
        iv1_max=cur["iv1_max"], iv1_min=cur["iv1_min"],
        iin_avg=(cur["iin_sum"] / d if d else np.nan),
        vo_final=cur["vo_final"], vo_max=cur["vo_max"],
        vo_under=_nan(cur["vo_under"]),
        vo_reg=(cur["vo_reg_sum"] / d if d else np.nan),
        p_in=p_in, p_out=p_out,
        efic_pct=(100.0 * p_out / p_in if d and p_in else np.nan),
    )
    return (info, np.array(cur["tp"]), np.array(cur["vop"]),
            np.array(cur["ilp"]), np.array(cur["iv1p"]))


def _nan(x):
    return x if np.isfinite(x) else np.nan


def main():
    print(f"Leyendo (streaming): {TXT_PATH}")
    if not TXT_PATH.exists():
        sys.exit(f"No existe: {TXT_PATH}")

    rows, waves = [], []
    for info, t, vo, il, iv1 in stream(TXT_PATH):
        info["caso"] = f"idx {info['idx']}: VIN={info['vin_dc']:.1f} -> VOUT={info['vout_target']:.1f}"
        info["il_peak_OK"] = info["il_step_max"] <= IL_PEAK_MAX
        rows.append(info)
        waves.append((info["caso"], t, vo, il, iv1))
        print(f"  idx {info['idx']}  {info['caso']:<38}  "
              f"il_step_max={info['il_step_max']:6.2f}A  "
              f"P_in={info['p_in']:7.2f}W  efic={info['efic_pct']:5.1f}%  ({len(t)} pts)")

    df = pd.DataFrame(rows).sort_values("idx")
    pd.set_option("display.width", 220)
    pd.set_option("display.max_columns", None)
    cols = ["idx", "caso", "il_max", "il_step_max", "il_step_min", "il_peak_OK",
            "iin_avg", "vo_reg", "vo_under", "p_in", "p_out", "efic_pct"]
    print("\n=== Resumen por esquina (V1) ===")
    print(df[cols].to_string(index=False))
    df.to_csv(OUT_CSV, index=False)
    print(f"\nTabla -> {OUT_CSV}")

    peligro = df[~df["il_peak_OK"]]
    if not peligro.empty:
        print(f"\nEsquinas que superan I_L_peak = {IL_PEAK_MAX} A en el escalon:")
        print(peligro[["caso", "il_step_max"]].to_string(index=False))

    # --- grafica: V(vout), I(L1), I(V1) de las 4 esquinas ---
    fig, axes = plt.subplots(3, 1, figsize=(11, 10), sharex=True)
    cmap = plt.cm.turbo(np.linspace(0, 1, len(waves)))
    for (caso, t, vo, il, iv1), col in zip(waves, cmap):
        if not t.size:
            continue
        axes[0].plot(t * 1e3, vo, lw=0.8, color=col, label=caso)
        axes[1].plot(t * 1e3, il, lw=0.8, color=col, label=caso)
        axes[2].plot(t * 1e3, iv1, lw=0.8, color=col, label=caso)
    axes[0].set_ylabel("V_OUT [V]")
    axes[0].set_title("Buckboost_V1 — 4 esquinas, escalon de carga 2A->8.4A @ 24 ms")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(fontsize=7, loc="center right")
    axes[1].axhline(IL_PEAK_MAX, color="red", ls="--", lw=1, label=f"{IL_PEAK_MAX} A pico")
    axes[1].axhline(IL_CONT_MAX, color="orange", ls=":", lw=1, label=f"{IL_CONT_MAX} A continuo")
    axes[1].set_ylabel("I_L1 [A]")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(fontsize=7, loc="upper right")
    axes[2].set_ylabel("I_V1 (entrada) [A]")
    axes[2].set_xlabel("Tiempo [ms]")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(fontsize=7, loc="upper right")
    fig.tight_layout()
    fig.savefig(OUT_PNG, dpi=150)
    print(f"Grafica -> {OUT_PNG}")


def _selftest():
    sample = (
        "time\tV(comp)\tV(fb)\tV(ss)\tV(vin)\tV(vout)\tI(L1)\tI(V1)\n"
        "Step Information: Idx=1  (Step: 1/4)\n"
        "0.0\t0\t0\t0\t6\t0.0\t0.0\t0.0\n"
        "0.025\t0\t0\t0\t6\t7.0\t13.0\t-70.0\n"      # dentro de W_STEP, W_UNDER
        "0.0272\t0\t0\t0\t6\t7.2\t4.0\t-60.0\n"      # dentro de W_PWR
        "0.0278\t0\t0\t0\t6\t7.2\t4.0\t-60.0\n"      # dentro de W_PWR
        "0.030\t0\t0\t0\t6\t7.2\t2.0\t-10.0\n"
        "Step Information: Idx=3  (Step: 3/4)\n"
        "0.0\t0\t0\t0\t22.2\t0.0\t0.0\t0.0\n"
        "0.0275\t0\t0\t0\t22.2\t25.0\t14.0\t-10.0\n"
    )
    p = Path(__file__).with_name("_selftest_v1.txt")
    p.write_text(sample)
    try:
        res = list(stream(p))
    finally:
        p.unlink()
    assert len(res) == 2, res
    i0 = res[0][0]
    assert i0["idx"] == 1 and i0["vin_dc"] == 6.0 and i0["vout_target"] == 7.2
    assert i0["il_max"] == 13.0
    assert i0["il_step_max"] == 13.0 and i0["il_step_min"] == 4.0  # 0.030 fuera de W_STEP
    assert i0["vo_under"] == 7.0
    # W_PWR: solo la fila t=0.0275 -> P_in = -6*-60 = 360 ; P_out = 7.2*(7.2/3.6 + 6.4)=7.2*8.4=60.48
    assert abs(i0["p_in"] - 360.0) < 1e-6, i0["p_in"]
    assert abs(i0["p_out"] - 60.48) < 1e-6, i0["p_out"]
    i1 = res[1][0]
    assert i1["idx"] == 3 and i1["vout_target"] == 25.0
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
    else:
        main()
