# Corre cada celda de un barrido .step como .cir independiente (una celda que no converge no bloquea
# el resto). Uso: python -I run_celdas.py S2b_snubber [timeout_s]
# Salida: celdas/<banco>__<celda>.cir/.log y celdas/<banco>_resultados.json
import re, subprocess, sys, os, json, time, itertools
from concurrent.futures import ThreadPoolExecutor

LT = r"C:\Program Files\ADI\LTspice\LTspice.exe"
bench = sys.argv[1]
TO = float(sys.argv[2]) if len(sys.argv) > 2 else 60
here = os.path.dirname(os.path.abspath(__file__))
CJO = sys.argv[3] if len(sys.argv) > 3 else None  # p.ej. 150p: solo para el ensayo de convergencia
out = os.path.join(here, "celdas" if not CJO else "celdas_Cjo" + CJO)
os.makedirs(out, exist_ok=True)
src = open(os.path.join(here, bench + ".cir")).read().replace("\r", "")
steps = re.findall(r"^\.step param (\w+) list (.*)$", src, re.M)
src = re.sub(r"^\.step param .*\n", "", src, flags=re.M)
src = src.replace("..\\modelos", "../../modelos")
if CJO:
    src = src.replace("Cjo=1p ", "Cjo=%s " % CJO)
names = [s[0] for s in steps]
cells = list(itertools.product(*[s[1].split() for s in steps]))


def mk(c):
    t = src
    for n, v in zip(names, c):
        t = re.sub(r"(^\.param .*?\b%s=)\S+" % n, lambda m: m.group(1) + v, t, count=1, flags=re.M)
    return t


def run(c):
    tag = bench + "__" + "_".join(f"{n}{v}" for n, v in zip(names, c))
    cir = os.path.join(out, tag + ".cir")
    open(cir, "w").write(mk(c))
    lg = os.path.join(out, tag + ".log")
    if os.path.exists(lg):
        os.remove(lg)
    t0 = time.time()
    p = subprocess.Popen([LT, "-b", cir], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        p.wait(TO)
    except subprocess.TimeoutExpired:
        p.kill()  # solo mi propio PID
        p.wait()
    log = open(lg, errors="ignore").read() if os.path.exists(lg) else ""
    m = {k: float(v) for k, v in re.findall(r"Measurement: (\w+)\n.*\n\s+1\s+(\S+)", log)}
    r = dict(cell=dict(zip(names, c)), done="Total elapsed" in log, secs=round(time.time() - t0, 1),
             relaxed=log.count("relaxed"), meas=m)
    print(json.dumps(r), flush=True)
    return r


with ThreadPoolExecutor(2) as ex:
    res = list(ex.map(run, cells))
json.dump(res, open(os.path.join(out, bench + "_resultados.json"), "w"), indent=1)
