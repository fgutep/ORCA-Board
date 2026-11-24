#!/usr/bin/env python
"""Control + registro de la carga Rigol DL3021 y la fuente EA PS 10060-170 para probar la placa LM5175.

  python carga.py --vin 12 cc 2.5 --t 60                  corriente constante 2.5 A por 60 s
  python carga.py --vin 12 steps 10% 25% 50% --dwell 30   escalones (A o % de la corriente maxima)
  python carga.py --vin 12 dyn 25% 75% --n 20             escalon repetido entre dos niveles
  python carga.py --vin 12 plan --vins 9 12 16.8        secuencia completa (~22 min)
  python carga.py report logs/<archivo>.csv               tabla + grafica

El script enciende la fuente a --vin (limite de corriente y protecciones OVP/OCP/OPP incluidos),
espera a que arranque la placa, conecta la carga y registra Vin, Iin, Vout, Iout, P a 20 Hz.
Seguridad: Pout <= 120 W, Iout <= --imax, aborta si Vout < 80 % del valor en vacio, si Vin cae
bajo 90 % de lo fijado (fuente en limite) o si el consumo en vacio es anormal.
Ctrl-C o cualquier error -> carga OFF y luego salida de la fuente OFF.
"""
import argparse, csv, datetime as dt, os, sys, time
import pyvisa, serial

PMAX, IMAX, VFLOOR, HZ, EFF = 120.0, 8.0, 0.8, 20, 0.85  # W, A, fraccion de V0, muestras/s, eficiencia supuesta
LOGS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")


class Fuente:
    """EA PS 10060-170 por SCPI serie (mismos comandos que PV-Emulator/comm/scpi.py)."""

    def __init__(self, port):
        self.s = serial.Serial(port, 115200, timeout=0.5)
        if "Elektro" not in self.q("*IDN?"):
            sys.exit(f"{port} no responde como EA-PS")

    def w(self, c):
        self.s.write((c + "\n").encode())

    def q(self, c):
        self.s.reset_input_buffer()
        self.w(c)
        return self.s.readline().decode().strip()

    def m(self, c):
        return float(self.q(c).split()[0])

    def config(self, v, vmax, ilim):
        for c in ("*CLS", "SYST:LOCK ON", "OUTP OFF"):  # sin pausa el firmware da -221
            self.w(c)
            time.sleep(0.15)
        # protecciones primero (estaban en 20 V / 2 A / 30 W), luego consignas
        for c in (f"VOLT:PROT {vmax*1.1+0.5:.2f}", f"CURR:PROT {ilim*1.15:.2f}", "POW:PROT 220",
                  f"VOLT {v:.2f}", f"CURR {ilim:.2f}", "POW 200"):
            self.w(c)
            time.sleep(0.05)
        e = self.q("SYST:ERR?")
        if not e.startswith("0"):
            sys.exit(f"La fuente rechazo la configuracion: {e!r}")

    def off(self):
        self.w("OUTP OFF")
        self.w("SYST:LOCK OFF")


class Carga:
    def __init__(self, nombre, slew, puerto):
        rm = pyvisa.ResourceManager()
        rid = next((r for r in rm.list_resources() if "0x1AB1" in r), None)  # Rigol
        if not rid:
            sys.exit("No se ve la DL3021 por USB")
        self.d = rm.open_resource(rid)
        self.d.timeout = 2000
        self.p = Fuente(puerto)
        self.d.write("SOUR:CURR:LEV:IMM 0")
        self.d.write("SOUR:INP:STAT 0")
        for c in ("SOUR:FUNC CURR", "SOUR:CURR:TRAN:MODE NONE", f"SOUR:CURR:SLEW {slew}"):
            self.d.write(c)
        os.makedirs(LOGS, exist_ok=True)
        self.path = os.path.join(LOGS, f"{dt.datetime.now():%Y%m%d_%H%M%S}_{nombre}.csv")
        self.f = open(self.path, "w", newline="")
        self.w = csv.writer(self.f)
        self.w.writerow(["t", "etapa", "iset", "v", "i", "p", "vin", "iin"])
        self.t0 = self.tset = self.tvin = time.perf_counter()
        self.iset, self.etapa = 0.0, ""

    def arranque(self, vin, vmax, ilim):
        """Enciende la fuente, espera a que la placa arranque y fija V0 / corriente maxima."""
        self.vin_set, self.ilim, self.vin_nom, self.vmax, self.bad = vin, ilim, vin, vmax, 0
        self.p.config(vin, vmax, ilim)
        # rampa de ~2 s en vez de un escalon: limita inrush y sobreimpulso en VBUS
        # (pasos de 0.2 s = intervalo minimo estable de la EA-PS segun PV-Emulator)
        self.p.w("VOLT 0")
        time.sleep(0.2)
        self.p.w("OUTP ON")
        for k in range(1, 11):
            time.sleep(0.2)
            self.p.w(f"VOLT {vin * k / 10:.2f}")
        self.tvin = time.perf_counter()
        prev = -1.0
        for _ in range(16):  # hasta 8 s
            time.sleep(0.5)
            v = self.v()
            if v > 3 and abs(v - prev) < 0.02:
                break
            prev = v
        else:
            sys.exit(f"La placa no arranca: Vout = {v:.2f} V con Vin = {vin} V (revise cableado / etapas 0-6 del bring-up)")
        iin = self.p.m("MEAS:CURR?")
        if iin > 1.0 or v > 27:
            sys.exit(f"ABORTA: en vacio Iin = {iin:.2f} A, Vout = {v:.2f} V")
        self.v0 = v
        self.icap = min(IMAX, PMAX / v)
        print(f"Vin = {vin} V (limite {ilim:.1f} A)  V0 = {v:.3f} V  Iin vacio = {iin:.2f} A  ->  Iout max = {self.icap:.2f} A ({self.icap*v:.0f} W)\nLog: {self.path}")

    def reinicia(self, etapa):
        """Tras un aborto: carga OFF, ciclo de la fuente y nuevo arranque (V0 se vuelve a medir)."""
        self.d.write("SOUR:CURR:LEV:IMM 0")
        self.d.write("SOUR:INP:STAT 0")
        self.iset = 0.0
        self.p.off()
        self.etapa = etapa
        time.sleep(5)
        self.arranque(self.vin_nom, self.vmax, self.ilim)
        self.hold(3, "reinicio")

    def cambia_vin(self, v):
        self.p.w(f"VOLT {v:.2f}")
        self.vin_set, self.tvin = v, time.perf_counter()

    def v(self):
        return float(self.d.query("MEAS:VOLT?"))

    def off(self):  # primero la carga, luego la fuente (aunque la carga ya no responda)
        try:
            self.d.write("SOUR:CURR:LEV:IMM 0")
            self.d.write("SOUR:INP:STAT 0")
        except Exception as e:
            print(f"  ! no se pudo apagar la carga ({e.__class__.__name__}): apagando la fuente igual")
        self.p.off()

    def rango(self, amps):  # solo con la entrada apagada
        self.d.write(f"SOUR:CURR:RANG {4 if amps <= 4 else 40}")

    def amps(self, tok):
        return float(tok[:-1]) / 100 * self.icap if tok.endswith("%") else float(tok)

    def set(self, a):
        if a > self.icap:
            print(f"  ! {a:.2f} A recortado a {self.icap:.2f} A (limite 120 W / --imax)")
            a = self.icap
        self.iset, self.tset = a, time.perf_counter()
        self.d.write(f"SOUR:CURR:LEV:IMM {a:.4f}")
        self.d.write(f"SOUR:INP:STAT {int(a > 0)}")

    def hold(self, s, etapa, avisos=None):
        """Mantiene el nivel actual s segundos registrando Vin, Iin, Vout, Iout, P."""
        self.etapa, avisos, badin, last = etapa, dict(avisos or {}), 0, 0.0
        ini = time.perf_counter()
        while (now := time.perf_counter()) - ini < s:
            v, i = self.v(), float(self.d.query("MEAS:CURR?"))
            vin, iin = self.p.m("MEAS:VOLT?"), self.p.m("MEAS:CURR?")
            self.w.writerow([f"{now-self.t0:.3f}", etapa, f"{self.iset:.4f}", f"{v:.4f}", f"{i:.4f}", f"{v*i:.3f}", f"{vin:.3f}", f"{iin:.3f}"])
            if v * i > PMAX * 1.03:
                sys.exit(f"ABORTA: P = {v*i:.0f} W > {PMAX:.0f} W")
            if self.iset > 0 and now - self.tset > 0.2:  # el contador persiste entre tramos (etapa dinamica = tramos de 0.5 s)
                self.bad = self.bad + 1 if v < VFLOOR * self.v0 else 0
            if self.bad >= 5:
                sys.exit(f"ABORTA: Vout = {v:.2f} V cayo bajo {VFLOOR:.0%} de {self.v0:.2f} V (limite de corriente / hiccup)")
            badin = badin + 1 if ((vin < 0.9 * self.vin_set or iin > 0.98 * self.ilim) and now - self.tvin > 1.0) else 0
            if badin >= 5:
                sys.exit(f"ABORTA: fuente en limite: Vin = {vin:.2f} V (fijado {self.vin_set}), Iin = {iin:.1f} A (limite {self.ilim:.1f})")
            for k in [k for k in avisos if now - ini >= k]:
                print(f"\n  >>> {avisos.pop(k)}")
            if now - last >= 1:
                print(f"  [{etapa}] {now-ini:4.0f}/{s}s Iset={self.iset:5.2f}A  Vout={v:7.3f}V Iout={i:6.3f}A Pout={v*i:6.1f}W | Vin={vin:6.2f}V Iin={iin:5.1f}A", end="\r")
                last = now
            time.sleep(max(0, 1 / HZ - (time.perf_counter() - now)))
        print()


def escalones(c, vals, dwell, pref):
    for a in vals:
        c.set(a)
        c.hold(dwell, f"{pref}_{c.iset:.2f}A")
    c.set(0)
    c.hold(3, "reposo")


def dinamico(c, lo, hi, ton, toff, n, pref):
    for _ in range(n):
        c.set(hi); c.hold(ton, f"{pref}_{lo:.1f}-{hi:.1f}A")
        c.set(lo); c.hold(toff, f"{pref}_{lo:.1f}-{hi:.1f}A")
    c.set(0)
    c.hold(3, "reposo")


def plan(c, a):
    pct = lambda p: p / 100 * c.icap

    def A():
        c.hold(20, "A_sin_carga")

    def B():
        escalones(c, [pct(p) for p in (10, 25, 50, 75, 100)], 40, "B_esc")

    def C():
        dinamico(c, pct(25), pct(75), .5, .5, 20, "C_din")
        dinamico(c, pct(50), pct(100), .5, .5, 20, "C_din")

    def E():
        for vin in a.vins:
            if c.v0 > 1.4 * vin:
                print(f"  ! Vin = {vin} V omitido: V0 = {c.v0:.1f} V > 1.4 x Vin (fuera de la regla boost)")
                continue
            c.cambia_vin(vin)
            c.hold(4, f"E_cambio_{vin}V")  # sin carga mientras la fuente cambia
            c.set(pct(a.line_pct))
            c.hold(60, f"E_linea_{vin}V")
            c.set(0)
        c.cambia_vin(a.vin)
        c.hold(4, "E_cambio_nominal")

    def D():
        c.set(c.icap)
        c.hold(a.soak, "D_soak_100%", {5: "soak: inicio", a.soak // 2: "soak: mitad", a.soak - 15: "soak: final"})
        c.set(0)
        c.hold(10, "reposo")

    abortos = 0
    for k in a.etapas.upper():
        try:
            dict(A=A, B=B, C=C, E=E, D=D)[k]()
        except SystemExit as e:  # apagado de la placa: se registra, se reinicia y se sigue con la etapa siguiente
            abortos += 1
            print(f"  !!! Etapa {k} abortada ({abortos}/{a.max_abortos}): {e}")
            if abortos > a.max_abortos:
                raise SystemExit(f"{abortos} abortos, se detiene: {e}")
            c.reinicia(f"ABORTO_{k}")


def report(path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import pandas as pd
    d = pd.read_csv(path)
    out = ["| Etapa | Iset [A] | Vin [V] | Iin [A] | Vout medio | Vout min | Vout max | Iout [A] | Pout [W] | Pérdidas [W] | Eficiencia |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    for e, g in d.groupby("etapa", sort=False):
        m = g.iloc[len(g) // 2:]  # 2a mitad = estado casi estable
        pin = (m.vin * m.iin).mean()
        eta = f"{m.p.mean()/pin:.1%}" if m.p.mean() > 1 and pin > 0 else "-"
        out.append(f"| {e} | {g.iset.max():.2f} | {m.vin.mean():.2f} | {m.iin.mean():.2f} | {m.v.mean():.3f} | {g.v.min():.3f} | {g.v.max():.3f} | {m.i.mean():.3f} | {m.p.mean():.1f} | {pin-m.p.mean():.1f} | {eta} |")
    e = d[d.etapa.str.startswith("B_esc")].groupby("etapa", sort=False).v.mean()
    if len(e) > 1:
        out.append(f"\nRegulacion de carga (escalones B): {(e.iloc[0]-e.iloc[-1])/e.iloc[0]:.2%} entre {e.index[0]} y {e.index[-1]}")
    txt = "\n".join(out)
    print(txt)
    open(path[:-4] + ".md", "w", encoding="utf-8").write(txt + "\n")
    fig, ax = plt.subplots(4, 1, sharex=True, figsize=(12, 10))
    for a_, cols, lab in zip(ax, (["v"], ["vin"], ["i", "iin"], ["p"]), ("Vout [V]", "Vin [V]", "Iout / Iin [A]", "Pout [W]")):
        for col in cols:
            a_.plot(d.t, d[col], lw=.7, label=col)
        a_.set_ylabel(lab); a_.grid(alpha=.3)
    ax[2].legend()
    ax[-1].set_xlabel("t [s]")
    fig.tight_layout()
    fig.savefig(path[:-4] + ".png", dpi=130)
    print("->", path[:-4] + ".md / .png")


def main():
    global IMAX
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--vin", type=float, help="Vin nominal [V] que fija la fuente (obligatorio salvo report)")
    p.add_argument("--ilim", type=float, help="limite de corriente de la fuente [A] (default: 1.25 x 120 W / (0.85 x Vin))")
    p.add_argument("--fuente", default="COM12", help="puerto serie de la EA-PS (default COM12)")
    p.add_argument("--slew", type=float, default=0.05, help="A/us (default 0.05)")
    p.add_argument("--imax", type=float, default=IMAX, help="tope de corriente [A] (default 8; 120 W a 12 V pide 10)")
    s = p.add_subparsers(dest="cmd", required=True)
    x = s.add_parser("cc"); x.add_argument("amps"); x.add_argument("--t", type=float, default=60)
    x = s.add_parser("steps"); x.add_argument("vals", nargs="+"); x.add_argument("--dwell", type=float, default=30)
    x = s.add_parser("dyn"); x.add_argument("lo"); x.add_argument("hi")
    x.add_argument("--ton", type=float, default=.5); x.add_argument("--toff", type=float, default=.5); x.add_argument("--n", type=int, default=20)
    x = s.add_parser("plan"); x.add_argument("--vins", type=float, nargs="+", default=[9, 12, 16.8])
    x.add_argument("--line-pct", type=float, default=30); x.add_argument("--soak", type=int, default=480)
    x.add_argument("--etapas", default="ABCED", help="letras de las etapas a correr, en orden (default ABCED)")
    x.add_argument("--max-abortos", type=int, default=0, help="apagados tolerados antes de detener todo (default 0: parar al primero, no re-energizar una placa con falla)")
    x = s.add_parser("report"); x.add_argument("csv")
    a = p.parse_args()
    if a.cmd == "report":
        return report(a.csv)
    if not a.vin:
        p.error("falta --vin")
    IMAX = a.imax
    vmax = max([a.vin] + (a.vins if a.cmd == "plan" else []))
    ilim = a.ilim or min(170.0, 1.25 * PMAX / (EFF * a.vin))
    c = Carga(a.cmd, a.slew, a.fuente)
    try:
        c.arranque(a.vin, vmax, ilim)
        if a.cmd == "cc":
            v = c.amps(a.amps); c.rango(v); c.set(v); c.hold(a.t, f"cc_{c.iset:.2f}A")
        elif a.cmd == "steps":
            v = [c.amps(t) for t in a.vals]; c.rango(max(v)); escalones(c, v, a.dwell, "esc")
        elif a.cmd == "dyn":
            lo, hi = c.amps(a.lo), c.amps(a.hi); c.rango(hi); dinamico(c, lo, hi, a.ton, a.toff, a.n, "din")
        elif a.cmd == "plan":
            c.rango(40); plan(c, a)
    finally:
        c.off()
        c.f.close()
        print("\nCarga y salida de la fuente OFF.")
        if os.path.getsize(c.path) > 100:
            report(c.path)
        else:
            os.remove(c.path)


if __name__ == "__main__":
    main()
