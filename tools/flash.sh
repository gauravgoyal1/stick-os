#!/usr/bin/env bash
set -euo pipefail
SKETCH="${1:-os}"
REPO_ROOT="$(git rev-parse --show-toplevel)"
FQBN="m5stack:esp32:m5stack_stickc_plus2"
PORT="/dev/cu.usbserial-5B1E0428761"

BUILD_PROPS=""
if [[ -f "$REPO_ROOT/partitions.csv" ]]; then
  BUILD_PROPS="--build-property build.partitions=partitions --build-property build.custom_partitions=$REPO_ROOT/partitions.csv"
fi

arduino-cli compile --fqbn "$FQBN" --libraries "$REPO_ROOT/libraries" $BUILD_PROPS "$SKETCH"
arduino-cli upload  --fqbn "$FQBN" --port "$PORT" "$SKETCH"
echo "done. to watch serial: arduino-cli monitor --port $PORT --config baudrate=115200"
