#!/usr/bin/env python3
"""ORCA-BOARD — monitor de telemetría por Wi-Fi para pruebas de potencia.

Consulta /status.json del firmware wifi_telemetry, imprime una línea por lectura,
guarda todo en CSV y avisa cuando la placa deja de responder o se reinicia.

Uso:
    python3 orca_monitor.py                         # AP de la placa: http://192.168.4.1
    python3 orca_monitor.py --host orca-board.local # si la placa está en tu red (modo STA)
    python3 orca_monitor.py --interval 0.2 --csv prueba_6S.csv

Solo usa la biblioteca estándar de Python.
"""

import argparse
import csv
import json
import sys
import time
import urllib.request
from datetime import datetime

FIELDS = [
    "uptime_ms", "boot_count", "reset_reason", "brownouts",
    "vbus_v", "vbus_min_v", "vbus_max_v", "ina226_ok", "pca9685_at_0x41",
    "isens_mv", "isens_a", "isens_max_a", "isens_zero_mv", "isens_zero_cal",
    "chip_temp_c", "free_heap", "min_free_heap", "wifi_mode", "ip", "rssi_dbm", "ap_clients", "samples",
]


def fmt(v, spec):
    return format("--", ">7s") if v is None else format(v, spec)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.4.1", help="IP o nombre de la placa (por defecto el AP: 192.168.4.1)")
    ap.add_argument("--interval", type=float, default=0.5, help="segundos entre lecturas (por defecto 0.5)")
    ap.add_argument("--timeout", type=float, default=1.5, help="timeout HTTP en segundos (por defecto 1.5)")
    ap.add_argument("--csv", default=None, help="archivo CSV (por defecto orca_power_<fecha>.csv)")
    args = ap.parse_args()

    url = f"http://{args.host}/status.json"
    csv_path = args.csv or datetime.now().strftime("orca_power_%Y%m%d_%H%M%S.csv")
    out = open(csv_path, "w", newline="")
    writer = csv.writer(out)
    writer.writerow(["pc_time", "event"] + FIELDS)

    print(f"Leyendo {url} cada {args.interval} s -> {csv_path}  (Ctrl+C para salir)")
    print(f"{'hora':8s}  {'uptime':>8s}  {'Vbus':>7s}  {'Vmin':>7s}  {'Isens':>7s}  {'Imax':>7s}  {'Tchip':>5s}  "
          f"{'RSSI':>4s}  boots  causa")

    last = None
    offline_since = None
    reboots = 0
    try:
        while True:
            t0 = time.time()
            now = datetime.now()
            try:
                with urllib.request.urlopen(url, timeout=args.timeout) as r:
                    j = json.load(r)
            except Exception as e:
                if offline_since is None:
                    offline_since = t0
                    print(f"{now:%H:%M:%S}  *** SIN RESPUESTA ({type(e).__name__})")
                    writer.writerow([now.isoformat(), "offline"] + [""] * len(FIELDS))
                    out.flush()
                time.sleep(max(0.0, args.interval - (time.time() - t0)))
                continue

            event = ""
            if offline_since is not None:
                print(f"{now:%H:%M:%S}  *** EN LÍNEA de nuevo tras {t0 - offline_since:.1f} s")
                offline_since = None
                event = "online"
            if last is not None and (j["boot_count"] != last["boot_count"] or j["uptime_ms"] < last["uptime_ms"]):
                reboots += 1
                event = f"{event}+reboot" if event else "reboot"
                print(f"{now:%H:%M:%S}  *** REINICIO #{reboots}: boot_count {last['boot_count']} -> "
                      f"{j['boot_count']}, causa {j['reset_reason']}, brownouts acumulados {j['brownouts']}")
            last = j

            print(f"{now:%H:%M:%S}  {j['uptime_ms'] / 1000:8.1f}  {fmt(j['vbus_v'], '7.3f')}  "
                  f"{fmt(j['vbus_min_v'], '7.3f')}  {j['isens_a']:7.3f}  {j['isens_max_a']:7.3f}  "
                  f"{j['chip_temp_c']:5.1f}  {j['rssi_dbm']:4d}  {j['boot_count']:5d}  {j['reset_reason']}")
            writer.writerow([now.isoformat(), event] + [j.get(k) for k in FIELDS])
            out.flush()
            time.sleep(max(0.0, args.interval - (time.time() - t0)))
    except KeyboardInterrupt:
        print(f"\nFin. Reinicios detectados: {reboots}. Log: {csv_path}")
    finally:
        out.close()


if __name__ == "__main__":
    sys.exit(main())
