"""
Analiza directamente el .raw binario mas reciente de LTspice (sin exportar a .txt).
Usa spicelib (ya instalado) para leer las trazas por paso.

Setup vigente (Buckboost_fixed.asc, corrida Sep 2 00:57-01:54):
    idx = 1 (fijo)              -> VIN_dc = 6 V, VOUT_target = 7.2 V
    .step param C8_val list 470u 680u          (capacitor de salida)
    ILOAD_base=2, ILOAD_target=10 -> ILOAD_delta=8
    Iload = PULSE(0 8 2m 1u 1u 1.5m 1)  -> escalon de carga 2A -> 10A en t=2ms,
                                            vuelve a 2A en t=3.5ms
    R7_val = VOUT_target/ILOAD_base = 3.6
    .tran 0 4.5m 0 100n

Reproduce las mismas ventanas que los .meas del .asc/.log.

Salidas:
    resumen_buckboost_raw.csv
    waveform_buckboost_raw.csv   (decimado, liviano para pegar en Claude web)
"""
import sys
from pathlib import Path
import numpy as np
from spicelib import RawRead

SIM = Path(__file__).resolve().parent.parent   # carpeta simulacion/
RAW_PATH = Path(
    sys.argv[1] if len(sys.argv) > 1 else
    SIM / "Buckboost_fixed.raw"
)
OUT_CSV = SIM / "resultados" / "c3_barrido_cout_raw_resumen.csv"
OUT_WAVE = SIM / "resultados" / "c3_barrido_cout_raw_waveform.csv"

VIN_DC = 6.0
VOUT_TARGET = 7.2
ILOAD_BASE = 2.0
ILOAD_TARGET = 10.0
ILOAD_DELTA = ILOAD_TARGET - ILOAD_BASE
R7_VAL = VOUT_TARGET / ILOAD_BASE

W_STEP = (0.002, 0.0035)
W_UNDER = (0.002, 0.0025)
W_PWR = (0.0032, 0.0035)
W_REG = (0.0017, 0.002)
W_OVER = (0.0035, 0.0045)

N_PLOT = 180  # puntos decimados por paso, para pegar en chat


def mask(t, a, b):
    return (t >= a) & (t <= b)


def avg_in_window(t, y, a, b):
    m = mask(t, a, b)
    if m.sum() < 2:
        return float("nan")
    return float(np.trapezoid(y[m], t[m]) / (t[m][-1] - t[m][0]))


def analyze_step(raw, step_idx, c8_val):
    t = raw.get_trace("time").get_wave(step_idx)
    t = np.abs(t)  # LTspice a veces exporta tiempo negativo en la vuelta del .tran
    vo = raw.get_trace("V(vout)").get_wave(step_idx)
    il = raw.get_trace("I(L1)").get_wave(step_idx)
    iv1 = raw.get_trace("I(V1)").get_wave(step_idx)

    m_step = mask(t, *W_STEP)
    m_under = mask(t, *W_UNDER)
    m_over = mask(t, *W_OVER)
    m_reg = mask(t, *W_REG)

    pin = -VIN_DC * iv1
    pout = vo * (vo / R7_VAL + np.where(t >= 0.002, ILOAD_DELTA, 0.0))

    ripple_pts = vo[m_reg]
    info = dict(
        c8_val=c8_val,
        il_step_max=float(il[m_step].max()),
        il_step_min=float(il[m_step].min()),
        vsense_peak=float((il[m_step] * 5e-3).max()),
        vo_under=float(vo[m_under].min()),
        vo_over=float(vo[m_over].max()),
        vo_load_avg=avg_in_window(t, vo, *W_PWR),
        vo_reg=avg_in_window(t, vo, *W_REG),
        p_in=avg_in_window(t, pin, *W_PWR),
        p_out=avg_in_window(t, pout, *W_PWR),
        vout_ripple_pre_pp_mV=float((ripple_pts.max() - ripple_pts.min()) * 1e3) if len(ripple_pts) else float("nan"),
    )
    info["efic_pct"] = 100.0 * info["p_out"] / info["p_in"] if info["p_in"] else float("nan")

    idx = np.linspace(0, len(t) - 1, N_PLOT).astype(int)
    idx = np.unique(idx)
    wave = (t[idx] * 1e3, vo[idx], il[idx])
    return info, wave


def main():
    print(f"Abriendo: {RAW_PATH}")
    raw = RawRead(str(RAW_PATH))
    steps = raw.steps if hasattr(raw, "steps") else raw.get_steps()
    print("pasos:", steps)

    cols = ["c8_val", "il_step_max", "il_step_min", "vsense_peak", "vo_under",
            "vo_over", "vo_load_avg", "vo_reg", "p_in", "p_out", "efic_pct",
            "vout_ripple_pre_pp_mV"]
    rows, waves = [], []
    for i, sdict in enumerate(steps):
        c8 = sdict["c8_val"] if isinstance(sdict, dict) else sdict
        info, wave = analyze_step(raw, i, c8)
        rows.append(info)
        waves.append(wave)
        print(f"  C8={c8*1e6:.0f}uF  il_step_max={info['il_step_max']:.2f}A  "
              f"vo_under={info['vo_under']:.3f}V  vo_over={info['vo_over']:.3f}V  "
              f"efic={info['efic_pct']:.1f}%  ripple_pre={info['vout_ripple_pre_pp_mV']:.1f}mV")

    with open(OUT_CSV, "w", encoding="utf-8") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            f.write(",".join(f"{r[c]:.6g}" for c in cols) + "\n")
    print(f"\nResumen -> {OUT_CSV}")

    with open(OUT_WAVE, "w", encoding="utf-8") as f:
        f.write("c8_val,t_ms,vout,il\n")
        for (c8, w) in zip((r["c8_val"] for r in rows), waves):
            tp, vop, ilp = w
            for ti, voi, ili in zip(tp, vop, ilp):
                f.write(f"{c8:.6g},{ti:.4f},{voi:.5f},{ili:.5f}\n")
    print(f"Forma de onda decimada -> {OUT_WAVE}")


if __name__ == "__main__":
    main()
