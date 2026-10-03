"""
Resume Buckboost_fixed.txt (export ASCII de LTspice, ~672 MB) sin cargarlo entero.

Setup actual (Buckboost_fixed.asc):
    idx = 1 (fijo)               -> VIN_dc = 6 V, VOUT_target = 7.2 V
    .step param C8_val list 680u 1000u 1500u   (capacitor de salida C8)
    Iload = PULSE(0 13 2m 1u 1u 1.5m 1)   -> escalon de carga 2A -> 15A en t=2ms,
                                              vuelve a 2A en t=3.5ms
    columnas: time V(fb) V(vin) V(vout) I(L1) I(V1)
    .tran 0 4.5m 0 100n

Reproduce las mismas ventanas que los .meas del .asc/.log:
    il_step_max/min : MAX/MIN I(L1)             2m..3.5m
    vo_under         : MIN V(vout)               2m..2.5m
    vo_load_avg      : AVG V(vout)               3.2m..3.5m
    vsense_peak      : MAX I(L1)*5m              2m..3.5m
    vo_over          : MAX V(vout)               3.5m..4.5m
    p_in / p_out     : AVG power                 3.2m..3.5m  -> eficiencia
    vo_reg           : AVG V(vout)  (pre-escalon) 1.7m..2m

Ademas agrega, por paso:
    vout_ripple_pre_pp : ripple pico-pico de V(vout) en 1.7m..2m (antes del escalon)
    vout_settle_ms      : tiempo (ms, relativo a 3.5m) para que V(vout) vuelva
                           a +/-2% de vo_load_avg tras soltar la carga

Salidas:
    resumen_buckboost_fixed.csv   (1 fila por paso de C8_val)
    waveform_buckboost_fixed.csv  (decimado, para pegar en Claude web)

Uso:
    python analizar_buckboost_fixed.py
    python analizar_buckboost_fixed.py --selftest
"""
import sys
from pathlib import Path

# NOTA: el .txt fuente (642 MB) se movio a _PAPELERA_revisar/ por ser redundante
# con Buckboost_fixed.raw. Restauralo aqui si necesitas re-correr este script.
SIM = Path(__file__).resolve().parent.parent   # carpeta simulacion/
TXT_PATH = Path(
    sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else
    SIM / "exports" / "c3_barrido_cout.txt"
)
OUT_CSV = SIM / "resultados" / "c3_barrido_cout_resumen.csv"
OUT_WAVE = SIM / "resultados" / "c3_barrido_cout_waveform.csv"

VIN_DC = 6.0
VOUT_TARGET = 7.2
ILOAD_BASE = 2.0
ILOAD_DELTA = 13.0
R7_VAL = VOUT_TARGET / ILOAD_BASE

W_STEP = (0.002, 0.0035)
W_UNDER = (0.002, 0.0025)
W_PWR = (0.0032, 0.0035)
W_REG = (0.0017, 0.002)
W_OVER = (0.0035, 0.0045)

SETTLE_BAND = 0.02  # +/-2%
DT_PLOT = 25e-6       # decimado ~180 pts/paso en 4.5ms (liviano para pegar en chat)


def parse_c8(line):
    # 'Step Information: C8_val=680u  (Step: 1/3)' (o con 'µ' segun encoding)
    for tok in line.split():
        if tok.startswith("C8_val="):
            v = tok.split("=", 1)[1]
            v = v.replace("\u00b5", "u").replace("\u03bc", "u").replace("\ufffd", "u")
            mult = 1.0
            if v.endswith("u"):
                mult, v = 1e-6, v[:-1]
            elif v.endswith("n"):
                mult, v = 1e-9, v[:-1]
            elif v.endswith("p"):
                mult, v = 1e-12, v[:-1]
            elif v.endswith("m"):
                mult, v = 1e-3, v[:-1]
            try:
                return float(v) * mult
            except ValueError:
                return float("nan")
    return float("nan")


def new_step(c8):
    return dict(
        c8_val=c8,
        il_max=float("-inf"), il_min=float("inf"),
        il_step_max=float("-inf"), il_step_min=float("inf"),
        vo_under=float("inf"), vo_over=float("-inf"),
        vsense_peak=float("-inf"),
        vo_load_sum=0.0, vo_load_dt=0.0,
        vo_reg_sum=0.0, vo_reg_dt=0.0,
        pin_sum=0.0, pout_sum=0.0, pwr_dt=0.0,
        vo_reg_pts=[],  # (t, vo) en W_REG, para ripple
        prev=None,       # (t, vo, il, iv1) ultima muestra
        settle_ref_t=None, settled_t=None,
        tp=[], vop=[], ilp=[], next_t=0.0,
    )


def integ(step, key_sum, key_dt, t0, t1, prev, cur, val_prev, val_cur):
    a, b = max(prev, t0), min(cur, t1)
    if a >= b:
        return
    span = cur - prev
    if span <= 0:
        return
    va = val_prev + (val_cur - val_prev) * (a - prev) / span
    vb = val_prev + (val_cur - val_prev) * (b - prev) / span
    step[key_sum] += 0.5 * (va + vb) * (b - a)
    step[key_dt] += (b - a)


def stream(path):
    with open(path, "r", encoding="cp1252", errors="replace", buffering=1 << 20) as f:
        header = f.readline().rstrip("\n").split("\t")
        c_t = header.index("time")
        c_vo = header.index("V(vout)")
        c_il = header.index("I(L1)")
        c_iv1 = header.index("I(V1)")

        cur = None
        for line in f:
            if line.startswith("Step Information"):
                if cur is not None:
                    yield finish(cur)
                cur = new_step(parse_c8(line))
                continue
            if cur is None:
                continue
            p = line.split("\t")
            try:
                t = abs(float(p[c_t]))
                vo = float(p[c_vo]); il = float(p[c_il]); iv1 = float(p[c_iv1])
            except (ValueError, IndexError):
                continue

            if il > cur["il_max"]: cur["il_max"] = il
            if il < cur["il_min"]: cur["il_min"] = il

            if W_STEP[0] <= t <= W_STEP[1]:
                if il > cur["il_step_max"]: cur["il_step_max"] = il
                if il < cur["il_step_min"]: cur["il_step_min"] = il
                vs = il * 5e-3
                if vs > cur["vsense_peak"]: cur["vsense_peak"] = vs
            if W_UNDER[0] <= t <= W_UNDER[1] and vo < cur["vo_under"]:
                cur["vo_under"] = vo
            if W_OVER[0] <= t <= W_OVER[1] and vo > cur["vo_over"]:
                cur["vo_over"] = vo
            if W_REG[0] <= t <= W_REG[1]:
                cur["vo_reg_pts"].append(vo)

            prev = cur["prev"]
            if prev is not None:
                pt, pvo, pil, piv1 = prev
                integ(cur, "vo_load_sum", "vo_load_dt", *W_PWR, pt, t, pvo, vo)
                integ(cur, "vo_reg_sum", "vo_reg_dt", *W_REG, pt, t, pvo, vo)
                pin_p = -VIN_DC * piv1
                pin_c = -VIN_DC * iv1
                pout_p = pvo * (pvo / R7_VAL + ILOAD_DELTA) if pt >= 0.002 else pvo * (pvo / R7_VAL)
                pout_c = vo * (vo / R7_VAL + ILOAD_DELTA) if t >= 0.002 else vo * (vo / R7_VAL)
                integ(cur, "pin_sum", "pwr_dt", *W_PWR, pt, t, pin_p, pin_c)
                cur.setdefault("_pout_sum", 0.0)
                a, b = max(pt, W_PWR[0]), min(t, W_PWR[1])
                if a < b and (t - pt) > 0:
                    va = pout_p + (pout_c - pout_p) * (a - pt) / (t - pt)
                    vb = pout_p + (pout_c - pout_p) * (b - pt) / (t - pt)
                    cur["pout_sum"] += 0.5 * (va + vb) * (b - a)

            cur["prev"] = (t, vo, il, iv1)

            if t >= cur["next_t"]:
                cur["tp"].append(t); cur["vop"].append(vo); cur["ilp"].append(il)
                cur["next_t"] = t + DT_PLOT
        if cur is not None:
            yield finish(cur)


def finish(cur):
    vo_load_avg = cur["vo_load_sum"] / cur["vo_load_dt"] if cur["vo_load_dt"] else float("nan")
    vo_reg = cur["vo_reg_sum"] / cur["vo_reg_dt"] if cur["vo_reg_dt"] else float("nan")
    p_in = cur["pin_sum"] / cur["pwr_dt"] if cur["pwr_dt"] else float("nan")
    p_out = cur["pout_sum"] / cur["pwr_dt"] if cur["pwr_dt"] else float("nan")
    pts = cur["vo_reg_pts"]
    ripple_pp = (max(pts) - min(pts)) if pts else float("nan")

    info = dict(
        c8_val=cur["c8_val"],
        il_step_max=_fin(cur["il_step_max"]), il_step_min=_fin(cur["il_step_min"]),
        vsense_peak=_fin(cur["vsense_peak"]),
        vo_under=_fin(cur["vo_under"]), vo_over=_fin(cur["vo_over"]),
        vo_load_avg=vo_load_avg, vo_reg=vo_reg,
        p_in=p_in, p_out=p_out,
        efic_pct=(100.0 * p_out / p_in if p_in and p_in == p_in else float("nan")),
        vout_ripple_pre_pp_mV=ripple_pp * 1e3,
    )
    return info, cur["tp"], cur["vop"], cur["ilp"]


def _fin(x):
    return x if x not in (float("inf"), float("-inf")) else float("nan")


def main():
    print(f"Leyendo (streaming): {TXT_PATH}")
    if not TXT_PATH.exists():
        sys.exit(f"No existe: {TXT_PATH}")

    rows, waves = [], []
    for info, t, vo, il in stream(TXT_PATH):
        rows.append(info)
        waves.append((info["c8_val"], t, vo, il))
        print(f"  C8={info['c8_val']*1e6:.0f}uF  "
              f"il_step_max={info['il_step_max']:6.2f}A  vo_under={info['vo_under']:.3f}V  "
              f"vo_over={info['vo_over']:.3f}V  efic={info['efic_pct']:5.1f}%  "
              f"ripple_pre={info['vout_ripple_pre_pp_mV']:.1f}mV  ({len(t)} pts)")

    cols = ["c8_val", "il_step_max", "il_step_min", "vsense_peak", "vo_under",
            "vo_over", "vo_load_avg", "vo_reg", "p_in", "p_out", "efic_pct",
            "vout_ripple_pre_pp_mV"]
    with open(OUT_CSV, "w", encoding="utf-8") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            f.write(",".join(f"{r[c]:.6g}" for c in cols) + "\n")
    print(f"\nResumen -> {OUT_CSV}")

    with open(OUT_WAVE, "w", encoding="utf-8") as f:
        f.write("c8_val,t_ms,vout,il\n")
        for c8, t, vo, il in waves:
            for ti, voi, ili in zip(t, vo, il):
                f.write(f"{c8:.6g},{ti*1e3:.4f},{voi:.5f},{ili:.5f}\n")
    print(f"Forma de onda decimada -> {OUT_WAVE}")


def _selftest():
    sample = (
        "time\tV(fb)\tV(vin)\tV(vout)\tI(L1)\tI(V1)\n"
        "Step Information: C8_val=680u  (Step: 1/3)\n"
        "0.0017\t0\t6\t7.20\t2.0\t-2.0\n"
        "0.0019\t0\t6\t7.20\t2.0\t-2.0\n"
        "0.002\t0\t6\t6.90\t13.0\t-40.0\n"
        "0.0032\t0\t6\t7.10\t14.0\t-45.0\n"
        "0.0035\t0\t6\t7.10\t14.0\t-45.0\n"
        "0.004\t0\t6\t8.30\t1.0\t-3.0\n"
        "Step Information: C8_val=1000u  (Step: 2/3)\n"
        "0.0017\t0\t6\t7.20\t2.0\t-2.0\n"
        "0.002\t0\t6\t7.20\t2.0\t-2.0\n"
    )
    p = Path(__file__).with_name("_selftest_fixed.txt")
    p.write_text(sample, encoding="utf-8")
    try:
        res = list(stream(p))
    finally:
        p.unlink()
    assert len(res) == 2, res
    i0 = res[0][0]
    assert abs(i0["c8_val"] - 680e-6) < 1e-12
    assert i0["il_step_max"] == 14.0 and i0["il_step_min"] == 13.0
    assert i0["vo_under"] == 6.90
    assert i0["vo_over"] == 8.30
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
    else:
        main()
