#!/bin/bash
# uso: _run2.sh "a b c" "d e"  -> dos cadenas secuenciales en paralelo (max 2 LTspice propios). Valida log; reintenta 1 vez.
LT="C:\Program Files\ADI\LTspice\LTspice.exe"
chain(){ for f in $1; do for t in 1 2 3; do rm -f $f.log $f.raw; "$LT" -b "$(cygpath -w "$PWD/$f.cir")"; grep -aq "Total elapsed time" $f.log && break; echo "$f incompleto intento $t"; done; done; }
chain "$1" & chain "$2" & wait
