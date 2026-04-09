#!/bin/zsh
set -euo pipefail

# Usage:
#   ./notify_board_mqtt.sh                -> send NEW MESSAGE
#   ./notify_board_mqtt.sh "HELLO"       -> send custom text

broker="broker.hivemq.com"
port="1883"
topic="uno-r4-topic"

msg="${*:-NEW MESSAGE}"
msg="${msg[1,16]}"

if ! command -v mosquitto_pub >/dev/null 2>&1; then
  echo "mosquitto_pub not found. Install with: brew install mosquitto"
  exit 1
fi

mosquitto_pub -h "$broker" -p "$port" -t "$topic" -m "$msg" -q 1
echo "Published to $topic: $msg"
