#!/usr/bin/env bash
# micro_ros_agent (ROS 2 Jazzy, Docker) para la placa ORCA.
#   ./agent.sh            agente por el USB de la placa (firmware orca_usb)
#   ./agent.sh udp        agente UDP en el puerto 8888 (firmware orca_wifi)
#   ./agent.sh usb -v6    con más detalle (vuelca el tráfico XRCE)
# --net=host e --ipc=host: los nodos ROS 2 del PC ven los tópicos de la placa (DDS por red y memoria compartida).
# Para USB solo se pasa ese puerto al contenedor (no --privileged ni /dev completo).
# Si quedó corriendo en segundo plano: docker stop orca_agent
set -e
MODE=${1:-usb}; shift || true
VERB=${1:--v4}
COMMON=(--rm --name orca_agent --net=host --ipc=host)
[ -t 0 ] && COMMON+=(-it)   # en una terminal: Ctrl-C cierra el contenedor
case "$MODE" in
  usb)
    PORT=/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:52:F6:2C-if00
    [ -e "$PORT" ] || { echo "No está $PORT: ¿placa conectada?" >&2; exit 1; }
    DEV=$(readlink -f "$PORT")   # docker --device no acepta los ':' del nombre by-id
    exec docker run "${COMMON[@]}" --device="$DEV:/dev/orca" microros/micro-ros-agent:jazzy \
      serial --dev /dev/orca -b 115200 "$VERB" ;;
  udp)
    exec docker run "${COMMON[@]}" microros/micro-ros-agent:jazzy udp4 --port 8888 "$VERB" ;;
  *) echo "uso: $0 [usb|udp] [-v4|-v6]" >&2; exit 2 ;;
esac
