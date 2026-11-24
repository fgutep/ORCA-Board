# Lee celdas/<banco>__*.log y arma una tabla (valor de cada .meas, estado). Uso: python -I leer_celdas.py S2b_snubber
import re, sys, os, glob
here = os.path.dirname(os.path.abspath(__file__))
b = sys.argv[1]
for f in sorted(glob.glob(os.path.join(here, "celdas", b + "__*.log"))):
    t = open(f, errors="ignore").read()
    m = {k: float(v) for k, v in re.findall(r"^(\w+): .*?=(\S+) (?:FROM|at)", t, re.M) if k != "vbus_pre"}
    ok = "Total elapsed" in t
    print(os.path.basename(f)[len(b) + 2:-4], "OK" if ok else "NO_CONVERGE(relaxed=%d)" % t.count("relaxed"),
          " ".join(f"{k}={v:.4g}" for k, v in m.items() if k in ("vbus_pk", "ipk", "i2t", "isn_pk", "isn_i2t", "vgs_min", "vds_rev")))
