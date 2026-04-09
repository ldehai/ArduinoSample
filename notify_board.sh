#!/bin/zsh
set -euo pipefail
setopt null_glob

# Usage:
#   ./notify_board.sh                -> send NEW_MSG
#   ./notify_board.sh "HELLO"       -> send MSG HELLO
#   ./notify_board.sh --list         -> list candidate serial ports

if [[ "${1:-}" == "--list" ]]; then
  local_ports=(/dev/cu.usbmodem* /dev/cu.usbserial* /dev/tty.usbmodem* /dev/tty.usbserial*)
  if [[ ${#local_ports[@]} -eq 0 ]]; then
    echo "No candidate serial ports found."
  else
    printf "%s\n" "${local_ports[@]}"
  fi
  exit 0
fi

# Prefer /dev/cu.* for outbound writes on macOS.
ports=(/dev/cu.usbmodem* /dev/cu.usbserial* /dev/tty.usbmodem* /dev/tty.usbserial*)
port="${ports[1]:-}"
if [[ -z "$port" ]]; then
  echo "No board serial port found."
  echo "Run: ./notify_board.sh --list"
  exit 1
fi

if [[ $# -eq 0 ]]; then
  payload="NEW_MSG"
else
  msg="$*"
  # Keep within LCD first-line width used by firmware.
  msg="${msg[1,16]}"
  payload="MSG ${msg}"
fi

# Configure line settings and send one command line.
stty -f "$port" 115200 cs8 -cstopb -parenb >/dev/null 2>&1 || true
printf "%s\n" "$payload" > "$port"

echo "Sent to $port: $payload"
