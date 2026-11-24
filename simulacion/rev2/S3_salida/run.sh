#!/bin/bash
# uso: bash run.sh caso.cir ...  Secuencial. Usa una copia privada de LTspice.exe (LTSPICE_EXE) porque otros bancos
# corren LTspice en paralelo y la instancia compartida aborta o desvia corridas -b. Reintenta si el log no termina.
EXE="${LTSPICE_EXE:-C:/Program Files/ADI/LTspice/LTspice.exe}"; PAT="$(basename "$EXE" .exe)"   # usar copia privada del exe (LTSPICE_EXE) si hay otros LTspice corriendo
w(){ until [ "$(tasklist | grep -c "${PAT}")" = "0" ]; do sleep 1; done; }
for f in "$@"; do b="${f%.cir}"
  for try in 1 2 3 4 5; do
    w; rm -f "$b.log" "$b.raw" "$b.op.raw" "$b.db" "$b.log.raw"
    "$EXE" -b "$(cygpath -w "$PWD/$f")"; w
    grep -q "Total elapsed time" "$b.log" 2>/dev/null && break
    echo "$f: intento $try incompleto, reintento"; sleep 2
  done
done
