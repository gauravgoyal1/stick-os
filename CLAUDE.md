# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repo overview

`stick-os` is a monorepo for the Stick OS platform — a launcher firmware for the M5StickC Plus 2 (ESP32), a MicroPython runtime for downloadable scripted apps, a FastAPI backend server, and tooling for building and publishing apps.

## Directory structure

- **`os/`** — Stick OS firmware (Arduino sketch). The main sketch is `os/os.ino`. Launcher logic split across `launcher_state.h`, `launcher_draw.cpp`, `launcher_nav.cpp`, `serial_cmd.cpp`. Partition table at `os/partitions.csv` (dual OTA + LittleFS) — auto-detected by the M5Stack platform.
- **`libraries/`** — Shared Arduino C++ libraries. Each app is a self-contained library that registers via `STICK_REGISTER_APP(...)`. Adding a new native app = create a library + add an `#include` in `os/os.ino`.
- **`libraries/micropython_vm/`** — Generated MicroPython v1.28.0 embed port + thin C++ wrapper. Regenerate via `./tools/build_mpy_vm.sh`. Do not edit the `src/extmod`, `src/genhdr`, `src/py`, `src/shared` subtrees — they get clobbered on regeneration.
- **`libraries/stick_os/`** — OS core: app registry (static + dynamic), app context, status strip, WiFi picker, stick store (NVS), stick fs (LittleFS), script host (MPY runner), stick ota (firmware update), app installer (LittleFS scanner + manifest parser).
- **`tools/user_c_modules/stick/`** — Canonical source for the Python `stick` module (`mod_stick.c`) and its C++ bindings (`stick_bindings.cpp`). `build_mpy_vm.sh` copies these into `libraries/micropython_vm/src/port/` after QSTR extraction.
- **`server/`** — FastAPI backend. Serves the app catalog, firmware updates, WiFi credentials, and Scribe WebSocket audio streaming. Runs as a single uvicorn process. Test suite under `server/tests/`. Production config (nginx, systemd) in `server/config/`.
- **`apps/`** — MicroPython scripted app sources (`snake`, `tilt`, plus dev `demo`). Each has `manifest.json` + `main.py`.
- **`docs/`** — Design specs and implementation plans.

`build_mpy_vm.sh` fetches MicroPython v1.28.0 on first run (outside the repo tree) and extracts the embed-port sources into `libraries/micropython_vm/`.

## Current apps (21 native + 2 reference scripted)

**Games (7):** `game_flappy`, `game_dino`, `game_scream`, `game_galaxy`, `game_balance`, `game_simon`, `game_panic` — each in `libraries/game_*/`
**Apps / Utilities (3):** `scribe` (audio streaming via WebSocket), `app_stopwatch`, `app_flashlight`
**Sensors (4):** `sensor_battery`, `sensor_imu`, `sensor_wifi_scan`, `sensor_mic`
**Settings (7):** `settings_about`, `settings_wifi`, `settings_storage`, `settings_apps`, `settings_time`, `settings_update` (firmware OTA), `app_store` (scripted-app installer)

**Scripted reference apps (`apps/`):** `snake` (game), `tilt` (utility). Constraint: scripted apps may only register in `CAT_GAME` or `CAT_UTILITY` — `registerApp()` rejects sensor/settings categories.

Shared game helpers live in `libraries/arcade_common/`.

## Build & Upload

```bash
# Compile + upload the OS firmware
./tools/flash.sh os

# Compile only (no upload)
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" os

# Regenerate the MicroPython VM sources (after editing mod_stick.c /
# stick_bindings.* / mpconfigport.h). Clones MicroPython on first run —
# set MPY_TAG=v1.28.0 (default) or any release tag.
./tools/build_mpy_vm.sh

# Find the device port
arduino-cli board list
```

**Board package sources** (configured in arduino-cli):
- ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
- M5Stack: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

**Required libraries**: M5Unified, M5StickCPlus2, ArduinoWebsockets.

## WiFi setup

WiFi credentials are stored in NVS (device flash), never in source code. `WIFI_SET` uses a tab delimiter internally so SSIDs with spaces work.

```bash
./tools/wifi_seed.py add --port /dev/cu.usbserial-XXXX --ssid "My Net" --password "secret"
./tools/wifi_seed.py list --port /dev/cu.usbserial-XXXX
./tools/wifi_seed.py delete --port /dev/cu.usbserial-XXXX --ssid "My Net"
```

On boot the OS tries the last connected network, then shows a WiFi picker if it fails.

## API key setup

Services (Scribe WebSocket) require a shared API key stored in NVS and validated by the server via `STICK_API_KEY` env var.

```bash
./tools/wifi_seed.py apikey-set --port /dev/cu.usbserial-XXXX --key "your-secret-key"
./tools/wifi_seed.py apikey-get --port /dev/cu.usbserial-XXXX
```

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
cp .env.example .env  # GEMINI_API_KEY, STICK_API_KEY, STICK_DOMAIN
uvicorn main:app --host 0.0.0.0 --port 8765

# Tests
pip install -r requirements-dev.txt
python3 -m pytest        # 17 tests, ~0.3s
```

**Production deployment:** nginx reverse-proxy and systemd user service configs are in `server/config/`. See `server/config/README.md` for setup instructions. Manage the server with:

```bash
systemctl --user restart stick-os-server
systemctl --user status stick-os-server
journalctl --user -u stick-os-server -f
```

**Endpoints:**
- `GET /api/catalog` — app catalog JSON (defaults to `{"version":1,"apps":[]}` when file missing)
- `GET /api/firmware` — firmware metadata (version / path / size / sha256)
- `GET /api/wifi` — WiFi credentials for device sync
- `GET /apps/{id}/{file}` — scripted-app downloads (static mount from `storage/apps/`)
- `GET /firmware/{file}` — firmware binary downloads
- `WS /services/scribe` — Scribe audio streaming + Gemini transcription

## Catalog format

Scripted apps served to the device in per-file form (no tar yet):

```json
{
  "version": 1,
  "apps": [
    {
      "id": "snake", "name": "Snake", "version": "1.0.0",
      "category": "game", "description": "...",
      "files": [
        {"name":"manifest.json", "url":"/apps/snake/manifest.json", "size":256, "sha256":"..."},
        {"name":"main.py",       "url":"/apps/snake/main.py",       "size":2000,"sha256":"..."}
      ]
    }
  ]
}
```

SHA256 is verified client-side during download; empty strings short-circuit the check (dev convenience).

## Scripted apps (MicroPython)

The `stick.*` Python API is declared in `tools/user_c_modules/stick/mod_stick.c` and implemented in `stick_bindings.cpp`. Surface (api_version 1):

```
stick.display.fill / rect / line / pixel / text / text2 / width / height
stick.buttons.update / a_pressed / b_pressed
stick.imu.accel / gyro
stick.store.get / put                     # per-app NVS
stick.millis / stick.delay / stick.exit
stick.BLACK WHITE RED GREEN BLUE YELLOW CYAN   # RGB565
```

Installation:
- `./tools/push_app.py --port ... --app apps/snake` — direct install over serial.
- Or use the **Store** native app to fetch via HTTPS from `/api/catalog`.

Runtime: `stick_os::scriptRunFile(path)` re-creates the MicroPython VM per launch (32 KB GC heap default), executes the .py source, tears down. xtensa uses setjmp-based NLR / GC-regs (not the default asm paths).

## Serial dev commands

Added to `os/serial_cmd.cpp` — all reach the device via `./tools/wifi_seed.py`-style raw writes or the `push_file.py` / `push_app.py` wrappers.

- `WIFI_SET <ssid>\t<password>` / `WIFI_LIST` / `WIFI_DEL <ssid>`
- `APIKEY_SET <key>` / `APIKEY_GET`
- `FILE_PUT <path> <size>\n<bytes>` — write a file to LittleFS (chunk the sender at ~128B with 20ms gaps; ESP32 UART buffer is ~256B).
- `FILE_LS <dir>` / `FILE_RM <path>`
- `MPY_RUN <path>` — execute a script via ScriptHost, bypassing the launcher UI (exception tracebacks go to serial).
- `APP_RM <id>` — uninstall a scripted app (unregister + recursive `rm`).

## Storage budget

The M5StickC Plus 2 has 8 MB flash.
- Firmware with MPY VM + bindings + OTA + App Store: ~1.54 MB (45% of 3 MB OTA slot).
- LittleFS partition: 1.9 MB for scripted apps.
- Baseline MPY VM adds ~126 KB flash. Each binding function adds ~200-500 bytes.
- Every feature must justify its flash cost — measure with `arduino-cli compile` after every change. Budget rule: > 50 KB for a non-core feature needs explicit justification.

## Conventions

- Apps register via `STICK_REGISTER_APP(...)` with an `AppDescriptor` struct (static/compile-time) or `stick_os::registerApp(descriptor*)` (runtime/dynamic — for LittleFS-installed scripted apps).
- Scripted apps must use `CAT_GAME` or `CAT_UTILITY`; registry rejects other categories.
- Apps use portrait mode (`setRotation(0)`) for launcher screens; games choose their own rotation.
- WiFi state goes through `StickNet::` — apps never `#include <WiFi.h>` (except stick_os internals).
- PWR-click exits any app via `stick_os::checkAppExit()` / `ArcadeCommon::updateAndCheckExit()`.
- Per-app storage uses `StickStore` (scoped NVS namespaces).
- Filesystem access uses `stick_os::fsReady() / fsTotalBytes() / fsUsedBytes()` or `LittleFS` directly (LittleFS is mounted by `stick_os::fsInit()` on boot).
- No Co-Authored-By lines in commits.
