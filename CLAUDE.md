# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repo overview

`stick-os` is a monorepo for the Stick OS platform — a launcher firmware for the M5StickC Plus 2 (ESP32), a FastAPI backend server, and tooling for building and publishing apps.

## Directory structure

- **`os/`** — Stick OS firmware (Arduino sketch). The main sketch is `os/os.ino`. Boots to a portrait-mode category picker (Games / Apps / Sensors / Settings) driven by the app registry in `libraries/stick_os/`.
- **`libraries/`** — Shared Arduino C++ libraries. Each app is a self-contained library that registers via `STICK_REGISTER_APP(...)`. Adding a new native app = create a library + add an `#include` in `os/os.ino`.
- **`server/`** — FastAPI backend. Serves the app catalog, firmware updates, WiFi credentials, and AiPin WebSocket audio streaming. Runs as a single uvicorn process.
- **`apps/`** — MicroPython app sources (Phase 2, not yet active).
- **`tools/`** — Build, flash, and publish tooling.
- **`docs/`** — Design specs and implementation plans.

## Current apps (19 native)

**Games (7):** `game_flappy`, `game_dino`, `game_scream`, `game_galaxy`, `game_balance`, `game_simon`, `game_panic` — each in `libraries/game_*/`
**Apps (3):** `aipin_wifi_app` (audio streaming via WebSocket), `app_stopwatch`, `app_flashlight`
**Sensors (4):** `sensor_battery`, `sensor_imu`, `sensor_wifi_scan`, `sensor_mic`
**Settings (5):** `settings_about`, `settings_wifi`, `settings_storage`, `settings_apps`, `settings_time`

Shared game helpers live in `libraries/arcade_common/`.

## Build & Upload

```bash
# Compile + upload the OS firmware
./tools/flash.sh os

# Compile only (no upload)
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" os

# Find the device port
arduino-cli board list
```

**Board package sources** (configured in arduino-cli):
- ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
- M5Stack: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

**Required libraries**: M5Unified, M5StickCPlus2, ArduinoWebsockets.

## WiFi setup

WiFi credentials are stored in NVS (device flash), never in source code. Three ways to add networks:

```bash
# Seed via USB serial
./tools/wifi_seed.py add --port /dev/cu.usbserial-XXXX --ssid "MyNetwork" --password "secret"

# List stored networks
./tools/wifi_seed.py list --port /dev/cu.usbserial-XXXX

# Delete a network
./tools/wifi_seed.py delete --port /dev/cu.usbserial-XXXX --ssid "MyNetwork"
```

On boot, the OS tries the last connected network, then shows a WiFi picker if it fails. Open/public networks are available as fallback.

## Device configuration

`libraries/stick_config/stick_config.h` (gitignored) contains the server host. Copy from `.h.example`:

```bash
cp libraries/stick_config/stick_config.h.example libraries/stick_config/stick_config.h
# Edit stick_config.h with your server host
```

## Server

```bash
cd server
pip install -r requirements.txt
cp .env.example .env  # fill in GEMINI_API_KEY, STICK_DOMAIN
uvicorn main:app --host 0.0.0.0 --port 8765
```

**Endpoints:**
- `GET /api/catalog` — app catalog JSON
- `GET /api/firmware` — firmware version JSON
- `GET /api/wifi` — WiFi credentials for device sync
- `GET /apps/{id}/{file}` — app package downloads
- `GET /firmware/{file}` — firmware binary downloads
- `WS /services/aipin` — AiPin audio streaming + Gemini transcription

## Storage optimization

The M5StickC Plus 2 has 8 MB flash. Current firmware: ~1.3 MB (39%). Every feature must justify its flash cost — measure compiled size after every change. Budget rule: > 50 KB for a non-core feature needs explicit justification.

## Conventions

- Apps register via `STICK_REGISTER_APP(...)` with an `AppDescriptor` struct
- Apps use portrait mode (`setRotation(0)`) for launcher screens; games choose their own rotation
- WiFi state goes through `StickNet::` — apps never `#include <WiFi.h>` (except stick_os internals)
- PWR-click exits any app via `stick_os::checkAppExit()` / `ArcadeCommon::updateAndCheckExit()`
- Per-app storage uses `StickStore` (scoped NVS namespaces)
- No Co-Authored-By lines in commits
