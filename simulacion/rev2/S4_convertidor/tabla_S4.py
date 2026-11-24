"""Tabla pasa/falla de S4b-e a partir de los .log (via meas.py). Uso: python tabla_S4.py  (en esta carpeta).
Estado: PASA / AL LIMITE (margen < 20 % del criterio) / FALLA. Un log no valido o ausente se marca 'sin dato'."""
import os
from meas import read

H = os.path.dirname(os.path.abspath(__file__))


def st(v, crit, lo=None):
    """v < crit PASA; margen < 20 % -> AL LIMITE; v >= crit FALLA (lo: umbral inferior si se da)"""
    if v is None:
        return "sin evento"
    return "PASA" if v < 0.8 * crit else ("AL LIMITE" if v < crit else "FALLA")


def log(name):
    p = os.path.join(H, name + ".log")
    if not os.path.exists(p):
        return None
    ok, m = read(p)
    return m if ok else "NO_VALIDO"


def row(case, what, val, crit, status):
    print(f"| {case} | {what} | {val} | {crit} | {status} |")


a = read(os.path.join(H, "S4a.log"))[1]
b = log("S4b_zA")
print("| Caso | Medida | Medido | Criterio | Estado |\n|---|---|---|---|---|")
if isinstance(b, dict):
    for k, lab in (("il_avg", "dif. il_avg vs S4a"), ("il_pk", "dif. il_pk vs S4a"), ("il_vy", "dif. il_vy vs S4a"), ("vout_reg", "dif. vout_reg vs S4a")):
        d = (b[k] / a[k] - 1) * 100
        row("S4b", lab, f"{d:+.2f} %", "< 2 %", st(abs(d), 2))
    row("S4b", "PGOOD antes de V(vout)=11 V", f"{b['t_pgood']*1e3:.3f} ms < {b['t_vout11']*1e3:.3f} ms", "al final del soft-start", "PASA" if b["t_pgood"] < b["t_vout11"] else "FALLA")
else:
    row("S4b", "-", "sin dato", "-", "sin dato")

TCEND = {"S4c_sin": 6e-3, "S4cd_con": 8.5e-3}  # fin de la ventana de cierre (TOPEN si hay apertura)
for name, label in (("S4c_sin", "S4c sin remedio"), ("S4cd_con", "S4c con remedio"),
                    ("S4e_7v4_10v", "S4e 7.4->10 V"), ("S4e_16v8_7v2", "S4e 16.8->7.2 V"), ("S4e_22v2_24v", "S4e 22.2->24 V")):
    m = log(name)
    if not isinstance(m, dict):
        row(label, "-", "sin dato" if m is None else "log no valido", "-", "sin dato")
        continue
    d = m["drop_c"]
    row(label + " (cierre)", "caida de V(vout) (mediana 50 ns)", f"{d:.2f} %", "< 9 % (ideal < 5 %)", st(d, 9) + (" (< 5 % ideal)" if d < 5 else ""))
    te = TCEND.get(name, 1e9)
    pg = m.get("pgf_c1") if (m.get("pgf_c1") or 1e9) < te else None
    go = m.get("goff_c1") if (m.get("goff_c1") or 1e9) < te else None  # un cruce despues de TOPEN es la apertura ordenada
    nore = pg is None and go is None
    row(label + " (cierre)", "rearme (PGOOD cae / Q6 reabre)", "ninguno" if nore else f"PGOOD cae a {pg*1e3:.3f} ms (min {m['pgmin_c']*1e3:.1f} mV)" if pg else "", "sin rearme", "PASA" if nore else "FALLA")
    if go: row(label + " (cierre)", "Q6 reabre", f"{go*1e3:.3f} ms", "sin rearme", "FALLA")
    v = m["vyl_max_c"]
    row(label + " (cierre)", "valle maximo de I(L1) tras el cierre", f"{v:.2f} A", "< 10.6 A", st(v, 10.6))
    if "os_o" in m:
        o = m["os_o"]
        row(label + " (apertura, S4d)", "valor previo / pico de V(vout)", f"{m['vpre_o']:.3f} V / {m['vmax_o']:.3f} V", "-", "informativo")
        row(label + " (apertura, S4d)", "sobreimpulso de V(vout) (mediana 50 ns)", f"{o:.2f} %", "< 7.5 % (ideal < 5 %)", st(o, 7.5) + (" (< 5 % ideal)" if o < 5 else ""))
