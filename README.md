# electronics-lab

A personal sandbox for small electronics projects, mostly on the **M5StickC Plus 2** (ESP32), plus breadboard / sensor experiments over time.

Each top-level directory is either a single-sketch mini app or an **umbrella** grouping related components. Sub-projects share some tooling (`tools/flash.sh`, `libraries/`) but otherwise don't depend on each other.

## Projects

| Dir | What it is | Status |
|---|---|---|
| [`aipin/`](aipin) | AiPin audio streamer — `bl/` (Bluetooth SPP), `wifi/` (WiFi + TCP), and a Python `server/` | Working |
| [`arcade/`](arcade) | Multi-game sketch (flappy, dino, scream, galaxy, balance, simon) | Working |
| [`clap_remote/`](clap_remote) | Clap-activated IR remote for a TCL Mini LED TV | 🚧 Phase 1 in progress |
| [`diagnostics/`](diagnostics) | Project-agnostic hardware test sketches — `hello_world/`, `ir_probe/`, `ir_sweep/` | Working |

## Shared tooling

- [`tools/flash.sh`](tools/flash.sh) — one-liner to `arduino-cli compile && upload` any sketch directory. Hard-codes the M5StickC Plus 2 FQBN and a USB port; edit those two lines if you use a different board or port. Passes `--libraries` so sketches can `#include` from `libraries/` at the repo root.
- `libraries/` — shared Arduino C++ libraries, empty day one and filled organically when a second sketch wants a helper. Some libraries are **gitignored config files**:
  - `libraries/wifi_config/wifi_config.h` — WiFi credentials (SSID/password priority list)
  - `libraries/secrets_config/secrets_config.h` — server endpoints and other runtime secrets

  Each has a tracked `.example` template. On a fresh clone, `cp *.h.example *.h` inside each dir and fill in real values before building any WiFi-using sketch.

## Hardware

Most projects target the [M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): ESP32-PICO-V3-02, built-in PDM mic, IR LED, 135×240 LCD, two buttons. Future projects may branch out to a bare ESP32 + breadboard + sensors.

## Convention

- Every top-level dir is either a single-sketch mini app (`arcade/`, `clap_remote/`) or an umbrella grouping related components (`aipin/` contains `bl/`, `wifi/`, `server/`; `diagnostics/` contains multiple test sketches). No nested `firmware/` subdirectory layer.
- Sketch dirs follow Arduino conventions: `<name>/<name>.ino`, with optional `.h`/`.cpp` siblings and an optional `docs/` subfolder (arduino-cli ignores non-source subdirs).
- Per-project notes live inside the project folder (`<project>/CLAUDE.md`, `<project>/README.md`).
- Root-level `CLAUDE.md` gives working notes to Claude / future-me for the whole repo.
- Superpowers workflow files (`docs/superpowers/` at any depth) are gitignored — specs and plans are session-scoped workspace artifacts, not tracked deliverables.

## Quick reference — build any sketch

```bash
./tools/flash.sh aipin/bl
./tools/flash.sh aipin/wifi
./tools/flash.sh arcade
./tools/flash.sh clap_remote
./tools/flash.sh diagnostics/ir_sweep
```

Compile-only (no upload):

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" \
  <sketch-dir>
```

See each sub-project's `CLAUDE.md` / `README.md` for project-specific setup (WiFi bootstrap, pairing, etc.).
