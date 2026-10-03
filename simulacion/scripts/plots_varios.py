#!/usr/bin/env python3
"""
ORCA-Board / Carga Luis (LM5175)
Rizado de V_OUT con y sin condensadores ceramicos de desacople.

Lee el .raw de LTspice de una corrida con  .step param CER list 1f 10u
y produce la figura de comparacion para la presentacion.

Uso:
    python plot_rizado.py Buckboost_fixed.raw

Opciones:
    --t0 1.150m --t1 1.160m     ventana a graficar (por defecto 10 us en regimen)
    --win 1.1m 1.2m             ventana para calcular pico-pico
    --trace V(vout)             senal a graficar

NOTA sobre la lectura del .raw
-------------------------------
Este script YA NO usa spicelib. spicelib detecta los pasos de un .step
leyendo el archivo .log que LTspice deja al lado del .raw (para saber
donde empieza y termina cada paso). Si ese .log no esta presente (se
borro, se movio el .raw solo, etc.) spicelib no encuentra pasos y
revienta con `RuntimeError: This RAW file does not have an axis.`

Para simulaciones .tran reales (no complejas) con la bandera
`stepped`, LTspice de todas formas escribe TODOS los pasos en un unico
bloque binario, uno atras del otro, y el unico indicio de donde
termina un paso y empieza el siguiente es que el tiempo vuelve a
bajar (se reinicia a 0). Por eso aqui parseamos el binario nosotros
mismos y cortamos los pasos donde detectamos ese reset de tiempo.
Esto es mas robusto: no depende de ningun archivo externo.
"""
import argparse, sys, struct
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --- paleta validada (categorica, slots 1 y 2) --------------------------------
C_SIN, C_CON = "#2a78d6", "#eb6834"
INK, INK2, INK3 = "#0b0b0b", "#52514e", "#8a8985"
SURFACE, GRID = "#fcfcfb", "#e4e3df"


def _si(x):
    """Convierte '1.150m', '10u', '1f' a float."""
    if isinstance(x, (int, float)):
        return float(x)
    s = str(x).strip()
    mult = {"f": 1e-15, "p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6,
            "m": 1e-3, "k": 1e3, "meg": 1e6}
    for suf in ("meg", "f", "p", "n", "u", "µ", "m", "k"):
        if s.lower().endswith(suf):
            return float(s[: -len(suf)]) * mult[suf]
    return float(s)


# --------------------------------------------------------------------------
#  Lector de .raw propio (sin spicelib) -- soporta .step de LTspice
# --------------------------------------------------------------------------
def _parse_header(raw_bytes):
    """Detecta si el header es UTF-16LE (normal en Windows) o ASCII,
    y devuelve (dict_de_campos, lista_de_variables, offset_binario, encoding)."""
    # LTspice casi siempre escribe el header en UTF-16LE en Windows.
    for enc in ("utf-16-le", "ascii", "latin-1"):
        marker = "Binary:\n".encode(enc) if enc != "ascii" else b"Binary:\n"
        idx = raw_bytes.find(marker)
        if idx != -1:
            header_bytes = raw_bytes[: idx + len(marker)]
            text = header_bytes.decode(enc, errors="replace")
            offset = idx + len(marker)
            return text, offset, enc
    sys.exit("No pude encontrar el marcador 'Binary:' en el .raw (¿archivo corrupto?).")


def _parse_fields(header_text):
    fields = {}
    variables = []
    lines = header_text.splitlines()
    in_vars = False
    for line in lines:
        line = line.rstrip("\r")
        if line.startswith("Variables:"):
            in_vars = True
            continue
        if in_vars:
            parts = line.strip().split("\t")
            if len(parts) >= 3 and parts[0].strip().isdigit():
                variables.append(parts[1].strip())
            continue
        if ":" in line:
            k, _, v = line.partition(":")
            fields[k.strip()] = v.strip()
    return fields, variables


def load_raw(path, trace="V(vout)"):
    """Devuelve [(t, v), ...] una entrada por paso de .step, leyendo el
    .raw binario directamente (no requiere el .log de LTspice)."""
    with open(path, "rb") as f:
        raw_bytes = f.read()

    header_text, offset, enc = _parse_header(raw_bytes)
    fields, variables = _parse_fields(header_text)

    if not variables:
        sys.exit("No pude leer la lista de variables del header del .raw.")

    match = next((v for v in variables if v.lower() == trace.lower()), None)
    if match is None:
        sys.exit(f"No encontre '{trace}'. Disponibles: {variables}")
    col = variables.index(match)

    flags = fields.get("Flags", "")
    is_complex = "complex" in flags
    npoints = int(fields.get("No. Points", "0"))
    nvars = int(fields.get("No. Variables", str(len(variables))))

    # Formato binario estandar de LTspice para datos reales (.tran, .op, .noise, etc):
    #   - la primera variable (tiempo / frecuencia) va en double (8 bytes)
    #   - el resto va en single float (4 bytes)
    # Para datos complejos (.ac) cada variable (incluida la primera) va en
    # 2 doubles (parte real + imaginaria).
    if is_complex:
        dtype = np.dtype([(f"v{i}", "<c16") for i in range(nvars)])
    else:
        dtype = np.dtype(
            [("v0", "<f8")] + [(f"v{i}", "<f4") for i in range(1, nvars)]
        )

    bytes_needed = npoints * dtype.itemsize
    available = len(raw_bytes) - offset
    if npoints <= 0 or bytes_needed > available:
        # No. Points del header no cuadra (o esta ausente): calculamos
        # cuantos puntos completos caben en el resto del archivo.
        npoints = available // dtype.itemsize
        bytes_needed = npoints * dtype.itemsize

    arr = np.frombuffer(raw_bytes, dtype=dtype, count=npoints, offset=offset)
    t_full = np.real(arr["v0"]).astype(float)
    v_full = np.real(arr[f"v{col}"]).astype(float) if col > 0 else t_full

    t_full = np.abs(t_full)

    # --- separar los pasos del .step ---
    # Si LTspice escribio un bloque "Plotname" por paso (comun en .ac / .noise)
    # ya vendrian separados, pero para .tran con la bandera "stepped" todos
    # los pasos quedan concatenados en un solo bloque y el tiempo se reinicia
    # (baja) al empezar cada paso nuevo. Detectamos esos reinicios.
    resets = np.where(np.diff(t_full) < 0)[0] + 1
    bounds = [0] + list(resets) + [len(t_full)]

    out = []
    for a, b in zip(bounds[:-1], bounds[1:]):
        if b - a < 2:
            continue
        out.append((t_full[a:b], v_full[a:b]))
    return out


def ripple(t, v, w0, w1, med_ns=50.0):
    """Pico-pico crudo y filtrado por mediana (mata picos de 1 paso de tiempo)."""
    m = (t >= w0) & (t <= w1)
    tw, vw = t[m], v[m]
    if len(tw) < 10:
        return np.nan, np.nan, np.nan
    ts = np.arange(tw[0], tw[-1], 2e-9)
    vr = np.interp(ts, tw, vw)
    w = max(3, int(med_ns * 1e-9 / 2e-9)) | 1
    pad = np.pad(vr, (w // 2, w // 2), mode="edge")
    vm = np.median(np.lib.stride_tricks.sliding_window_view(pad, w), axis=-1)
    return vw.max() - vw.min(), vm.max() - vm.min(), vw.mean()


def _medfilt(t, v, t0, t1, med_ns=50.0):
    m = (t >= t0) & (t <= t1)
    tw, vw = t[m], v[m]
    ts = np.arange(tw[0], tw[-1], 2e-9)
    vr = np.interp(ts, tw, vw)
    w = max(3, int(med_ns * 1e-9 / 2e-9)) | 1
    pad = np.pad(vr, (w // 2, w // 2), mode="edge")
    return ts, np.median(np.lib.stride_tricks.sliding_window_view(pad, w), axis=-1)


def _estilo(ax):
    ax.grid(True, color=GRID, lw=0.8, zorder=0)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK3, labelsize=9.5, length=0)


def figura(series, t0, t1, w0, w1, out="rizado_vout"):
    """series = [(etiqueta, color, t, v), ...] en orden: sin, con."""
    stats = [(lb, c, *ripple(t, v, w0, w1)) for lb, c, t, v in series]

    fig, (axA, axB) = plt.subplots(2, 1, figsize=(9.4, 6.9), dpi=200,
                                   sharex=True,
                                   gridspec_kw=dict(hspace=0.28))
    fig.patch.set_facecolor(SURFACE)

    for ax in (axA, axB):
        ax.set_facecolor(SURFACE)
        _estilo(ax)

    for (lb, color, t, v) in series:
        m = (t >= t0) & (t <= t1)
        axA.plot((t[m] - t0) * 1e6, v[m], color=color, lw=1.6,
                 solid_capstyle="round", zorder=3)
        ts, vm = _medfilt(t, v, t0, t1)
        axB.plot((ts - t0) * 1e6, vm, color=color, lw=2.0,
                 solid_capstyle="round", zorder=3)

    n = lambda k: "   ".join(f"{s[0]} {s[k]*1e3:.0f} mV pp" for s in stats)
    axA.set_title(f"A · señal cruda — los cerámicos eliminan los picos de conmutación"
                  f"          {n(2)}",
                  color=INK, fontsize=10.2, fontweight="600", loc="left", pad=7)
    axB.set_title(f"B · filtrada a 50 ns — el rizado de fondo NO cambia"
                  f"          {n(3)}",
                  color=INK, fontsize=10.2, fontweight="600", loc="left", pad=7)

    axB.set_xlabel("tiempo  [µs]", color=INK2, fontsize=10)
    for ax in (axA, axB):
        ax.set_ylabel("V(vout)  [V]", color=INK2, fontsize=10)
    axA.set_xlim(0, (t1 - t0) * 1e6)

    leg = fig.legend([plt.Line2D([], [], color=c, lw=2.4) for _, c, *_ in stats],
                     [lb for lb, *_ in stats], loc="upper left",
                     bbox_to_anchor=(0.012, 0.907), frameon=False,
                     fontsize=9.8, labelcolor=INK2, handlelength=1.7, ncol=2,
                     columnspacing=2.2)
    leg.set_zorder(6)

    fig.suptitle("Rizado de V_OUT: qué arreglan y qué no arreglan los cerámicos",
                 color=INK, fontsize=13.5, fontweight="600", x=0.012, ha="left", y=0.985)
    fig.text(0.012, 0.948,
             f"El rizado real solo baja {stats[0][3]/stats[1][3]:.2f}× — los ~{stats[1][3]*1e3:.0f} mV "
             "de fondo son alternancia buck↔boost, no filtrado insuficiente.",
             color=INK2, fontsize=10, va="top")
    fig.text(0.012, 0.012,
             "LM5175 · V_IN 12 V · V_OUT 11.98 V · f_sw 350 kHz · zona de transición · carga 8.4 A   ·   "
             "los picos del panel A duran ~5 ns y el modelo no tiene parásitas de layout: su amplitud no es predictiva",
             color=INK3, fontsize=7.6)

    fig.subplots_adjust(left=0.085, right=0.985, top=0.805, bottom=0.10, hspace=0.30)
    for ext in ("png", "pdf"):
        fig.savefig(f"{out}.{ext}", facecolor=SURFACE, bbox_inches="tight")
    print(f"  -> {out}.png / {out}.pdf")

    print("\n  serie              pp crudo    pp real    V medio")
    for lb, _, crudo, filt, medio in stats:
        print(f"  {lb:<18} {crudo*1e3:7.1f} mV  {filt*1e3:7.1f} mV  {medio:7.4f} V")
    print(f"\n  mejora sobre el rizado real: {stats[0][3]/stats[1][3]:.2f}x")
    return fig


def main():
    p = argparse.ArgumentParser()
    p.add_argument("raw")
    p.add_argument("--trace", default="V(vout)")
    p.add_argument("--t0", default="1.150m")
    p.add_argument("--t1", default="1.160m")
    p.add_argument("--win", nargs=2, default=["1.1m", "1.2m"])
    p.add_argument("--out", default="rizado_vout")
    a = p.parse_args()

    datos = load_raw(a.raw, a.trace)
    if len(datos) < 2:
        sys.exit("El .raw trae un solo paso. Falta el '.step param CER list 1f 10u'.")
    etiquetas = [("sin cerámicos", C_SIN), ("con cerámicos", C_CON)]
    series = [(lb, c, t, v) for (lb, c), (t, v) in zip(etiquetas, datos)]
    figura(series, _si(a.t0), _si(a.t1), _si(a.win[0]), _si(a.win[1]), a.out)


if __name__ == "__main__":
    main()