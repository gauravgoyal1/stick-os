#!/usr/bin/env bash
set -euo pipefail
SKETCH="${1:-}"
if [[ -z "$SKETCH" ]]; then
  echo "usage: $0 <sketch-dir>  e.g. $0 clap_remote" >&2
  exit 1
fi
REPO_ROOT="$(git rev-parse --show-toplevel)"
FQBN="m5stack:esp32:m5stack_stickc_plus2"
PORT="/dev/cu.usbserial-5B1E0428761"
arduino-cli compile --fqbn "$FQBN" --libraries "$REPO_ROOT/libraries" "$SKETCH"
arduino-cli upload  --fqbn "$FQBN" --port "$PORT" "$SKETCH"
echo "done. to watch serial: arduino-cli monitor --port $PORT --config baudrate=115200"
