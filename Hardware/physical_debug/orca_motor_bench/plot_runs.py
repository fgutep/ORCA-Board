#!/usr/bin/env python3
"""ORCA-BOARD — gráficas y tabla de las secuencias del banco de motores.

Lee los CSV que descarga el panel (/log.csv) y genera:
  - curvas_corriente_duty.png: I_alim en régimen contra |duty|, adelante (línea continua) y atrás (discontinua)
    para cada ensayo de tipo sweep
  - series_tiempo.png: duty del canal y corriente de VM contra el tiempo, una fila por ensayo
  - resumen.md: tabla de régimen por paso (media ± desviación, pico de 10 ms, I_motor estimada)

El régimen de cada paso es la media de `isens_a` en la última ventana (--window s) con el duty ya en su
valor nominal: coincide con la ventana de medida de la secuencia si --window = "medir" del panel.

Uso:
    python3 plot_runs.py "N20 libre=datos/N20.csv" "FM libre=datos/FM.csv" --out graficas/
    python3 plot_runs.py "FM 50 kHz=fm50k.csv" --window 2

Requiere matplotlib.
"""

import argparse
import csv
import os
import re
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Paleta categórica validada (orden fijo; ver la skill de visualización): azul, naranja, aqua, amarillo,
# magenta, verde, violeta, rojo. Aqua y amarillo tienen poco contraste sobre fondo claro: por eso las curvas
# llevan etiqueta directa y el resumen va en tabla.
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948"]
SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3de"
LOG_HZ = 10


def load(path):
    rows = list(csv.DictReader(open(path, newline="")))
    for r in rows:
        for k in ("t_s", "duty1", "duty2", "isens_a", "isens_max_a", "isens_min_a"):
            r[k] = float(r[k])
        r["step"] = int(r["step"])
    return rows


def steps_of(rows, window_s):
    """Agrupa por paso y calcula el régimen de cada paso con duty nominal (sweep, soak, step...)."""
    groups = {}
    for r in rows:
        groups.setdefault(r["step"], []).append(r)
    out = []
    last_step = max(groups)
    for k, rs in groups.items():
        lab = rs[0]["label"]
        m_ch, m_d = re.search(r"M([12])", lab), re.search(r"([+-]?\d+)%", lab)
        if not (m_ch and m_d):
            continue
        ch, nominal = int(m_ch.group(1)), float(m_d.group(1))
        key = f"duty{ch}"
        at = [r for r in rs if abs(r[key] - nominal) < 0.05]
        if not at:
            continue
        win = at[-max(1, round(window_s * LOG_HZ)):]
        i = [r["isens_a"] for r in win]
        mean = statistics.mean(i)
        out.append({
            "label": lab, "ch": ch, "duty": nominal, "n": len(win), "i": mean, "sd": statistics.pstdev(i),
            "peak": win[-1]["isens_max_a"], "min": win[-1]["isens_min_a"],
            "imotor": mean / (abs(nominal) / 100) if abs(nominal) >= 20 else None,
            # Si el último paso termina con el duty aún en el nominal, la secuencia se cortó (HALT/STOP)
            "incomplete": k == last_step and abs(rs[-1][key] - nominal) < 0.05 and "sweep" in lab,
        })
    return out


def style(ax):
    ax.set_facecolor(SURFACE)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(INK2)
    ax.tick_params(colors=INK2, labelsize=9)
    ax.yaxis.label.set_color(INK2)
    ax.xaxis.label.set_color(INK2)


def plot_curves(runs, path):
    fig, ax = plt.subplots(figsize=(9, 5.4), dpi=150, facecolor=SURFACE)
    style(ax)
    ends = []
    for idx, (name, rows, st) in enumerate(runs):
        sw = [s for s in st if "sweep" in s["label"]]
        if not sw:
            continue
        c = SERIES[idx % len(SERIES)]
        for sign, ls, mk in ((1, "-", "o"), (-1, "--", "s")):
            pts = sorted((abs(s["duty"]), s["i"], s["sd"], s["incomplete"]) for s in sw if s["duty"] * sign > 0)
            if not pts:
                continue
            x, y, e = [p[0] for p in pts], [p[1] for p in pts], [p[2] for p in pts]
            ax.errorbar(x, y, yerr=e, color=c, linestyle=ls, linewidth=2, marker=mk, markersize=6,
                        markeredgecolor=SURFACE, markeredgewidth=1.5, capsize=3, elinewidth=1, zorder=3)
            for xi, yi, _, inc in pts:
                if inc:  # punto de un paso cortado: hueco
                    ax.plot(xi, yi, marker=mk, markersize=11, markerfacecolor="none", markeredgecolor=c, zorder=4)
            if sign == 1:
                ends.append([x[-1], y[-1], name, c])
    # Etiquetas directas al final de la curva "adelante", separadas si se pisan
    ends.sort(key=lambda t: t[1])
    span = ax.get_ylim()[1] - ax.get_ylim()[0]
    for k in range(1, len(ends)):
        ends[k][1] = max(ends[k][1], ends[k - 1][1] + 0.06 * span)
    for x, y, name, c in ends:
        ax.annotate(name, (x, y), xytext=(10, 0), textcoords="offset points", va="center", fontsize=9,
                    color=INK, bbox=dict(boxstyle="round,pad=0.2", fc=SURFACE, ec=c, lw=1.5))
    ax.set_xlim(0, 125)
    ax.set_xticks(range(0, 101, 20))
    ax.set_ylim(bottom=0)
    ax.set_xlabel("|duty| (%)")
    ax.set_ylabel("I de alimentación en VM, régimen (A)")
    ax.set_title("Corriente de VM contra duty — sweep, media de la ventana de medida ± desviación",
                 loc="left", fontsize=11, color=INK)
    handles = [plt.Line2D([], [], color=SERIES[i % len(SERIES)], linewidth=2, label=n) for i, (n, _, _) in
               enumerate(runs)]
    handles += [plt.Line2D([], [], color=INK2, linestyle="-", marker="o", label="adelante (+)"),
                plt.Line2D([], [], color=INK2, linestyle="--", marker="s", label="atrás (−)"),
                plt.Line2D([], [], color=INK2, linestyle="none", marker="o", markersize=11, markerfacecolor="none",
                           label="paso cortado")]
    leg = ax.legend(handles=handles, loc="upper left", fontsize=8.5, frameon=True, facecolor=SURFACE,
                    edgecolor=GRID, labelcolor=INK)
    leg.get_frame().set_linewidth(0.8)
    fig.tight_layout()
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)


def plot_series(runs, path):
    n = len(runs)
    fig, axes = plt.subplots(n + 1, 1, figsize=(10, 1.9 * (n + 1)), dpi=150, sharex=True, facecolor=SURFACE,
                             gridspec_kw={"height_ratios": [0.8] + [1] * n})
    ref = runs[0][1]
    ch = runs[0][2][0]["ch"] if runs[0][2] else 2
    ax = axes[0]
    style(ax)
    ax.plot([r["t_s"] for r in ref], [r[f"duty{ch}"] for r in ref], color=INK2, linewidth=1.5)
    ax.set_ylabel(f"duty M{ch} (%)", fontsize=9)
    ax.set_title(f"Duty comandado ({runs[0][0]}; igual en todos) y corriente de VM a 10 Hz", loc="left",
                 fontsize=11, color=INK)
    ymax = max(max(r["isens_a"] for r in rows) for _, rows, _ in runs) * 1.1
    for idx, (name, rows, _) in enumerate(runs):
        ax = axes[idx + 1]
        style(ax)
        c = SERIES[idx % len(SERIES)]
        ax.plot([r["t_s"] for r in rows], [r["isens_a"] for r in rows], color=c, linewidth=1.6)
        ax.set_ylim(min(0, min(r["isens_a"] for r in rows)) - 0.02, ymax)
        ax.set_ylabel("I (A)", fontsize=9)
        ax.text(0.005, 0.92, name, transform=ax.transAxes, va="top", fontsize=9, color=INK,
                bbox=dict(boxstyle="round,pad=0.2", fc=SURFACE, ec=c, lw=1.5))
    axes[-1].set_xlabel("tiempo desde el inicio de la secuencia (s)")
    fig.tight_layout()
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)


def summary_md(runs, window_s):
    lines = [f"Régimen = media de `isens_a` en el último {window_s:g} s de cada paso con el duty en su valor nominal. "
             "I_motor ≈ I_alim / duty (solo con duty ≥ 20 %).", ""]
    for name, rows, st in runs:
        lines += [f"**{name}** ({len(rows)} filas, {rows[-1]['t_s']:.1f} s)", "",
                  "| Paso | I_alim (A) | ± (A) | Pico 10 ms (A) | Mín. 10 ms (A) | I_motor est. (A) |",
                  "|---|---|---|---|---|---|"]
        for s in st:
            im = "—" if s["imotor"] is None else f"{s['imotor']:.3f}"
            note = " ⚠️ cortado" if s["incomplete"] else ""
            lines.append(f"| {s['label']}{note} | {s['i']:.3f} | {s['sd']:.3f} | {s['peak']:.3f} | {s['min']:.3f} | {im} |")
        lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("runs", nargs="+", help='"nombre=archivo.csv", en el orden de los colores')
    ap.add_argument("--out", default=".", help="carpeta de salida (por defecto la actual)")
    ap.add_argument("--window", type=float, default=1.0, help='ventana de régimen en s (= "medir"; por defecto 1)')
    args = ap.parse_args()

    runs = []
    for spec in args.runs:
        name, _, path = spec.partition("=")
        if not path:
            ap.error(f"'{spec}' no es nombre=archivo.csv")
        rows = load(path)
        runs.append((name, rows, steps_of(rows, args.window)))
    os.makedirs(args.out, exist_ok=True)
    plot_curves(runs, os.path.join(args.out, "curvas_corriente_duty.png"))
    plot_series(runs, os.path.join(args.out, "series_tiempo.png"))
    md = summary_md(runs, args.window)
    with open(os.path.join(args.out, "resumen.md"), "w") as f:
        f.write(md + "\n")
    print(md)
    print(f"-> {args.out}/curvas_corriente_duty.png, series_tiempo.png, resumen.md")


if __name__ == "__main__":
    main()
