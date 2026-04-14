# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repo overview

`electronics-lab` is a sandbox for small electronics / M5StickC Plus 2 / ESP32 projects. Each top-level directory is either a single-sketch mini app or an **umbrella** grouping related components (one sketch plus helpers, or several related sketches). Sub-projects share some tooling (`tools/flash.sh`, `libraries/`) but otherwise don't depend on each other.

## Current sub-projects

- **`aipin/`** — AiPin audio streamer sub-project. Two firmware variants (`bl/` Bluetooth SPP, `wifi/` WiFi+TCP) plus a Python host-side server under `server/`. The WiFi variant's implementation lives in `libraries/aipin_wifi_app/`; `aipin/wifi/wifi.ino` is a thin wrapper for solo flashing. The canonical production target is `stick/`. See `aipin/CLAUDE.md` for architecture and build notes.
- **`arcade/`** — Multi-game arcade sketch for the M5StickC Plus 2 (flappy, dino, scream, galaxy, balance, simon). The implementation lives in `libraries/arcade_app/`; `arcade/arcade.ino` is a thin wrapper so `./tools/flash.sh arcade` still flashes the game standalone for solo debugging. The canonical production target is `stick/`.
- **`clap_remote/`** — Clap-activated IR remote for a TCL Mini LED TV. Phase 1 in progress. See `clap_remote/CLAUDE.md` and `clap_remote/docs/superpowers/` for spec, plan, and working notes.
- **`diagnostics/`** — Project-agnostic hardware test sketches: `hello_world/`, `ir_probe/`, `ir_sweep/`, `wifi_scan/`. Good for first-time peripheral sanity checks or hardware probes that aren't tied to any one project.
- **`stick/`** — Unified launcher firmware that bundles Arcade and AiPin-WiFi into one binary. Boot shows a menu; BtnA selects, BtnB cycles, a short power-button click inside any app restarts back to the launcher (last selection remembered via `Preferences`). The individual sketches in `arcade/` and `aipin/wifi/` still flash standalone for solo debugging. Production target for the StickC Plus 2.

## Shared libraries convention

Shared Arduino C++ libraries live in `libraries/` at the repo root. `tools/flash.sh` passes this directory to `arduino-cli compile` via `--libraries`, so any sketch can `#include` from it without extra configuration. The directory starts empty and fills organically when a second sketch wants a helper — there is no speculative sharing.

Some libraries under `libraries/` are **gitignored config files**: `wifi_config/wifi_config.h` holds WiFi credentials and `secrets_config/secrets_config.h` holds server endpoints and other runtime secrets. Each has a tracked `.example` template; on a fresh clone, `cp *.h.example *.h` and fill in your values. Sketches include them via `#include <wifi_config.h>` / `#include <secrets_config.h>` and the `--libraries` flag on `flash.sh` handles discovery.

## Build & Upload

Uses `arduino-cli` with the M5Stack board package. `tools/flash.sh` is the usual entry point for any Arduino sketch in the repo.

```bash
# Compile + upload in one shot
./tools/flash.sh stick           # production target: arcade + aipin/wifi launcher
./tools/flash.sh arcade          # solo arcade
./tools/flash.sh aipin/wifi      # solo aipin/wifi
./tools/flash.sh aipin/bl
./tools/flash.sh clap_remote
./tools/flash.sh diagnostics/ir_probe

# Compile only, no upload
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" \
  <sketch-dir>

# Find the device port
arduino-cli board list
```

**Board package sources** (configured in arduino-cli):
- ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
- M5Stack: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

**Required libraries**: M5Unified, M5StickCPlus2, BluetoothSerial (ESP32 core, BT variant only), WiFi (ESP32 core, WiFi variant only), IRremote (clap_remote, ir_probe, ir_sweep), arduinoFFT (clap_remote detector).

## Host-side tooling

See `aipin/CLAUDE.md` for the Python receivers (`aipin/bl/receiver.py`, `aipin/server/server.py`) and the `GEMINI_API_KEY` requirement for transcription.
