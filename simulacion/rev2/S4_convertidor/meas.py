"""Lee los .meas de un .log de LTspice y valida el log. Uso: python meas.py S4b.log   (imprime nombre=valor; FALLA si FAIL'ed)"""
import re, sys

NUM = r"[-+]?\d+\.?\d*(?:[eE][-+]?\d+)?"


def read(log):
    txt = open(log, encoding="latin-1", errors="replace").read()
    if "\x00" in txt:  # UTF-16 (error de apertura)
        txt = open(log, encoding="utf-16", errors="replace").read()
    ok = (txt.count("Total elapsed time") == 1 and "tolerance relaxed" not in txt)
    m = {}
    for line in txt.splitlines():
        r = re.match(r"^(\w+): (.*)$", line)
        if not r or r.group(1) in ("Warning", "Note", "WARNING", "Instance", "Circuit", "Options", "Total"):
            continue
        name, body = r.groups()
        a = re.search(r"\bAT (" + NUM + r")\s*$", body)  # WHEN: valor = instante
        if a:
            m[name] = float(a.group(1))
            continue
        a = re.search(r"=\s*(" + NUM + r")(?:\s+(?:FROM|TO|at)\b.*)?\s*$", body)
        if a:
            m[name] = float(a.group(1))
    for f in re.findall(r'Measurement "(\w+)" FAIL', txt):
        m[f.lower()] = None
    return ok, m


if __name__ == "__main__":
    ok, m = read(sys.argv[1])
    print("LOG VALIDO" if ok else "LOG NO VALIDO (sin 'Total elapsed time' unico o con 'tolerance relaxed')")
    for k, v in m.items():
        print(f"{k} = {v if v is not None else 'FAIL (sin evento)'}")
