"""Dibujos de las placas ORCA a partir de los gerbers de fabricacion.

Genera SVG en dos estilos: lineal (como los dibujos tecnicos del manual) y a color
(mascara, cobre, pads y serigrafia), en vista superior e isometrica, con marcadores
numerados sobre los conectores (posiciones tomadas del .kicad_pcb, fp.json).
"""
import json, math, re, warnings
from gerbonara import GerberFile, ExcellonFile

warnings.filterwarnings("ignore")
R = "C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/"
FP = json.load(open("fp.json"))
ACC = "#1FB57A"

BOARDS = {
    "ctrl": dict(
        d=R + "Hardware/Quote_fab/ORCA-BOARD-",
        edge="Edge_Cuts", fcu="F_Cu", fmask="F_Mask", fsilk="F_Silkscreen",
        drills=["PTH.drl", "NPTH.drl"], rot=-90,
        marks=[("J19", 1), ("J20", 2), ("J1", 3), ("J3", 4), ("J4", 5), ("J6", 6),
               ("J15", 7), ("J5", 8), ("J2", 9), ("U2", 10), ("U1", 11)],
    ),
    "pot": dict(
        d=R + "Hardware-Potencia/jlcpcb/gerber/ORCA-BOARD-",
        edge="EdgeCuts", fcu="CuTop", fmask="MaskTop", fsilk="SilkTop",
        drills=["PTH.drl", "NPTH.drl"], rot=0,
        marks=[("J1", 1), ("J2", 2), ("RV1", 3), ("U1", 4), ("Q3", 5), ("L1", 6)],
    ),
}


def inner(svg):
    """Contenido del <g> de gerbonara, en coordenadas gerber (y hacia arriba)."""
    s = str(svg)
    a = s.index(">", s.index("<g ")) + 1
    b = s.rindex("</g>")
    return s[a:b]


def recolor(body, fill=None, stroke=None):
    if fill is not None:
        body = body.replace('fill="black"', f'fill="{fill}"')
    if stroke is not None:
        body = body.replace('stroke="black"', f'stroke="{stroke}"')
    return body


def layer(cfg, name):
    return inner(GerberFile.open(cfg["d"] + name + ".gbr").to_svg())


def drills(cfg):
    out = ""
    for f in cfg["drills"]:
        try:
            out += inner(ExcellonFile.open(cfg["d"] + f).to_svg())
        except Exception as e:  # ponytail: un archivo de taladro raro no debe tumbar el dibujo
            print("drill", f, e)
    return out


def outline_pts(edge_body):
    nums = [float(v) for v in re.findall(r"-?\d+\.?\d*", " ".join(re.findall(r'd="([^"]+)"', edge_body)))]
    return [(nums[i], -nums[i + 1]) for i in range(0, len(nums) - 1, 2)]  # a coordenadas KiCad


def build(key):
    cfg = BOARDS[key]
    edge = layer(cfg, cfg["edge"])
    pts = outline_pts(edge)
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    L = dict(
        edge=edge, cu=layer(cfg, cfg["fcu"]), mask=layer(cfg, cfg["fmask"]),
        silk=layer(cfg, cfg["fsilk"]), drl=drills(cfg), cu_line=(key == "pot"),
    )
    # contorno como poligono (el Edge_Cuts viene como trazos)
    poly = " ".join(f"{x:.3f},{y:.3f}" for x, y in pts)
    return cfg, L, (x0, x1, y0, y1, cx, cy), poly


def board_group(L, style, poly):
    """Placa en coordenadas KiCad (x derecha, y abajo)."""
    flip = 'transform="scale(1,-1)"'
    if style == "line":
        return f"""
<polygon points="{poly}" fill="#ffffff" stroke="#111" stroke-width="0.35" stroke-linejoin="round"/>
{f'<g {flip}>' + recolor(L['cu'], fill='#eceeed', stroke='#eceeed') + '</g>' if L.get('cu_line') else ''}
<g {flip}>{recolor(L['mask'], fill='#cfd3d1')}</g>
<g {flip}>{recolor(L['silk'], fill='#222', stroke='#222')}</g>
<g {flip}>{recolor(L['drl'], fill='#fff')}</g>
<g {flip} fill="none">{recolor(L['drl'], fill='none').replace('<circle', '<circle stroke="#111" stroke-width="0.12"')}</g>
<polygon points="{poly}" fill="none" stroke="#111" stroke-width="0.35" stroke-linejoin="round"/>"""
    return f"""
<polygon points="{poly}" fill="#0E6B3F" stroke="#0A4E2E" stroke-width="0.3"/>
<g {flip} opacity="0.55">{recolor(L['cu'], fill='#1E9A5C', stroke='#1E9A5C')}</g>
<g {flip}>{recolor(L['mask'], fill='#D8B25A', stroke='#D8B25A')}</g>
<g {flip}>{recolor(L['silk'], fill='#F4F4F0', stroke='#F4F4F0')}</g>
<g {flip}>{recolor(L['drl'], fill='#151515')}</g>"""


def marker(x, y, n, r=2.3):
    return (f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{ACC}" stroke="#fff" stroke-width="0.5"/>'
            f'<text x="{x:.2f}" y="{y + r * 0.42:.2f}" text-anchor="middle" font-family="Segoe UI, Arial, sans-serif" '
            f'font-weight="700" font-size="{r * 1.15:.2f}" fill="#fff">{n}</text>')


def rot(p, c, deg):
    a = math.radians(deg)
    x, y = p[0] - c[0], p[1] - c[1]
    return (c[0] + x * math.cos(a) - y * math.sin(a), c[1] + x * math.sin(a) + y * math.cos(a))


def top(key, style, marks=False, pad=6):
    cfg, L, (x0, x1, y0, y1, cx, cy), poly = build(key)
    w, h = x1 - x0, y1 - y0
    if cfg["rot"] in (90, -90):
        w, h = h, w
    vb = f"{cx - w / 2 - pad} {cy - h / 2 - pad} {w + 2 * pad} {h + 2 * pad}"
    m = ""
    if marks:
        for ref, n in cfg["marks"]:
            x, y = FP[key][ref][:2]
            x, y = rot((x, y), (cx, cy), cfg["rot"])
            m += marker(x, y, n)
    g = board_group(L, style, poly)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}">'
            f'<g transform="rotate({cfg["rot"]} {cx} {cy})">{g}</g>{m}</svg>')


def iso(key, style, thick=2.2):
    """Vista isometrica: proyeccion afin (vectorial) con espesor extruido."""
    cfg, L, (x0, x1, y0, y1, cx, cy), poly = build(key)
    c30, s30 = math.cos(math.radians(30)), math.sin(math.radians(30))
    M = (c30, s30, -c30, s30)

    def P(x, y):
        x, y = rot((x, y), (cx, cy), cfg["rot"])
        x, y = x - cx, y - cy
        return (M[0] * x + M[2] * y, M[1] * x + M[3] * y)

    pts = [P(*map(float, q.split(","))) for q in poly.split()]
    sx, sy = [p[0] for p in pts], [p[1] for p in pts]
    pad = 6
    vb = f"{min(sx) - pad} {min(sy) - pad} {max(sx) - min(sx) + 2 * pad} {max(sy) - min(sy) + thick + 2 * pad}"
    side_fill = "#ffffff" if style == "line" else "#0A4E2E"
    side_stroke = "#111" if style == "line" else "#083d24"
    bot = " ".join(f"{x:.3f},{y + thick:.3f}" for x, y in pts)
    # aristas verticales en los extremos izquierdo, derecho e inferior de la silueta
    ext = [min(pts, key=lambda p: p[0]), max(pts, key=lambda p: p[0]), max(pts, key=lambda p: p[1])]
    vert = "".join(f'<line x1="{x:.3f}" y1="{y:.3f}" x2="{x:.3f}" y2="{y + thick:.3f}" stroke="{side_stroke}" stroke-width="0.35"/>' for x, y in ext)
    shadow = " ".join(f"{x + 3:.3f},{y + thick + 3:.3f}" for x, y in pts)
    a, b, c, d = M
    tf = f'matrix({a} {b} {c} {d} 0 0) translate({-cx} {-cy}) rotate({cfg["rot"]} {cx} {cy})'
    sh = "" if style == "line" else f'<polygon points="{shadow}" fill="#000" opacity="0.12" filter="url(#b)"/>'
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}">'
            f'<defs><filter id="b"><feGaussianBlur stdDeviation="1.6"/></filter></defs>{sh}'
            f'<polygon points="{bot}" fill="{side_fill}" stroke="{side_stroke}" stroke-width="0.35" stroke-linejoin="round"/>{vert}'
            f'<g transform="{tf}">{board_group(L, style, poly)}</g></svg>')


def exploded(key, names, gap=26, thick=1.2):
    """Capas de cobre apiladas en isometrico (vista explosionada del stackup)."""
    cfg, L, (x0, x1, y0, y1, cx, cy), poly = build(key)
    c30, s30 = math.cos(math.radians(30)), math.sin(math.radians(30))
    M = (c30, s30, -c30, s30)

    def P(x, y):
        x, y = rot((x, y), (cx, cy), cfg["rot"])
        x, y = x - cx, y - cy
        return (M[0] * x + M[2] * y, M[1] * x + M[3] * y)

    pts = [P(*map(float, q.split(","))) for q in poly.split()]
    sx, sy = [p[0] for p in pts], [p[1] for p in pts]
    n = len(names)
    pad = 6
    vb = f"{min(sx) - pad} {min(sy) - pad} {max(sx) - min(sx) + 2 * pad} {max(sy) - min(sy) + gap * (n - 1) + thick + 2 * pad}"
    a, b, c, d = M
    tf = f'matrix({a} {b} {c} {d} 0 0) translate({-cx} {-cy}) rotate({cfg["rot"]} {cx} {cy})'
    out = ""
    for i, (fname, color) in reversed(list(enumerate(names))):
        dy = i * gap
        body = layer(cfg, fname)
        bot = " ".join(f"{x:.3f},{y + dy + thick:.3f}" for x, y in pts)
        out += (f'<g><polygon points="{bot}" fill="#fff" stroke="#111" stroke-width="0.3"/>'
                f'<g transform="translate(0 {dy}) {tf}"><polygon points="{poly}" fill="#fff" fill-opacity="0.93" stroke="#111" stroke-width="0.35"/>'
                f'<g transform="scale(1,-1)">{recolor(body, fill=color, stroke=color)}</g></g></g>')
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}">{out}</svg>'


if __name__ == "__main__":
    import os
    os.makedirs("img", exist_ok=True)
    for k in BOARDS:
        open(f"img/{k}_line_top.svg", "w").write(top(k, "line", marks=True))
        open(f"img/{k}_line_plain.svg", "w").write(top(k, "line"))
        open(f"img/{k}_color_top.svg", "w").write(top(k, "color"))
        open(f"img/{k}_line_iso.svg", "w").write(iso(k, "line"))
        open(f"img/{k}_color_iso.svg", "w").write(iso(k, "color"))
    open("img/ctrl_stack.svg", "w").write(exploded("ctrl", [
        ("F_Silkscreen", "#222"), ("F_Cu", ACC), ("In1_Cu", "#9a9a9a"), ("In2_Cu", "#7d8fa0"), ("B_Cu", "#444")]))
    open("img/pot_stack.svg", "w").write(exploded("pot", [
        ("SilkTop", "#222"), ("CuTop", ACC), ("CuIn1", "#9a9a9a"), ("CuIn2", "#7d8fa0"), ("CuBottom", "#444")], gap=22))
    print("ok")
