#!/usr/bin/env bash
# Clone MicroPython and generate the embed-port C sources into
# libraries/micropython_vm/. Spike script for Task 2b — paths may need
# adjustment as MicroPython's ports/embed makefile evolves.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
MPY_DIR="$REPO_ROOT/third_party/micropython"
OUT_DIR="$REPO_ROOT/libraries/micropython_vm"
MPY_TAG="${MPY_TAG:-v1.25.0}"

mkdir -p "$REPO_ROOT/third_party"

if [[ ! -d "$MPY_DIR" ]]; then
  echo "[build_mpy_vm] cloning MicroPython $MPY_TAG..."
  git clone --depth 1 --branch "$MPY_TAG" \
    https://github.com/micropython/micropython.git "$MPY_DIR"
fi

echo "[build_mpy_vm] building mpy-cross..."
make -C "$MPY_DIR/mpy-cross" -j4

EMBED_PORT="$MPY_DIR/ports/embed"
BUILD_DIR="$MPY_DIR/examples/embedding"
if [[ ! -d "$EMBED_PORT" || ! -d "$BUILD_DIR" ]]; then
  echo "ERROR: embed port or example dir missing in the cloned tree." >&2
  exit 1
fi

# v1.28.0: the embed port is driven from a caller Makefile that includes
# ports/embed/embed.mk. examples/embedding/ provides exactly that setup.
# We override its mpconfigport.h with ours and run only the package
# generation target (micropython-embed-package), not the host example.
cp "$REPO_ROOT/tools/mpconfigport.h" "$BUILD_DIR/mpconfigport.h"

USER_MOD_DIR="$REPO_ROOT/tools/user_c_modules"

echo "[build_mpy_vm] generating embed port sources (with user modules)..."
(cd "$BUILD_DIR" && make -f micropython_embed.mk USER_C_MODULES="$USER_MOD_DIR" clean || true)
(cd "$BUILD_DIR" && make -f micropython_embed.mk USER_C_MODULES="$USER_MOD_DIR")

GEN_DIR="$BUILD_DIR/micropython_embed"
if [[ ! -d "$GEN_DIR" ]]; then
  echo "ERROR: cannot locate generated micropython_embed dir." >&2
  exit 1
fi

# Arduino 1.5 library layout: generated C sits under src/, our wrapper
# (micropython_vm.h/.cpp) and library.properties stay at the library root.
SRC_DIR="$OUT_DIR/src"
echo "[build_mpy_vm] copying generated sources to $SRC_DIR..."
mkdir -p "$SRC_DIR"
# Clean only the generated subdirs — never touch src/micropython_vm.*.
for d in extmod genhdr port py shared; do
  rm -rf "$SRC_DIR/$d"
done
cp -R "$GEN_DIR"/* "$SRC_DIR/"

# Copy user module sources into the Arduino library so arduino-cli
# compiles them as part of the micropython_vm library. QSTRs have
# already been baked into the generated qstrdefs by the embed build.
for modDir in "$USER_MOD_DIR"/*/; do
  modName=$(basename "$modDir")
  echo "[build_mpy_vm] installing user module '$modName' into $SRC_DIR/port/"
  find "$modDir" -maxdepth 1 \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) \
    -exec cp {} "$SRC_DIR/port/" \;
done

n_c=$(find "$SRC_DIR" -name '*.c' | wc -l | tr -d ' ')
n_h=$(find "$SRC_DIR" -name '*.h' | wc -l | tr -d ' ')
echo "[build_mpy_vm] done — $n_c .c files, $n_h .h files in $SRC_DIR"
