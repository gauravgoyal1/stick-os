# stick-os

Launcher firmware for the **M5StickC Plus 2** (ESP32) — a tiny wearable computer with a portrait-mode category picker, 21 native apps, WiFi connectivity, a MicroPython runtime for downloadable scripted apps, and a FastAPI backend for catalog + OTA updates.

![status](https://img.shields.io/badge/status-phase%202-brightgreen)
![platform](https://img.shields.io/badge/platform-ESP32--PICO--V3--02-blue)
![flash](https://img.shields.io/badge/flash-46%25%20of%203MB-informational)
![license](https://img.shields.io/badge/license-MIT-green)

## About

**stick-os** turns the M5StickC Plus 2 — a $20 ESP32-powered wearable the size of a USB stick — into a small handheld computer with a real launcher. Boot it and you get a category picker (Games / Apps / Sensors / Settings); click into a category and a scrollable list shows every installed app with its icon. The OS owns an 18-pixel status strip at the top (WiFi, clock, battery) that apps are forbidden to draw over — giving every app a consistent frame without per-app code.

What's in the box:

- **21 native apps** covering arcade games, utilities, sensor readouts, and system settings, all compiled into the firmware (46% of a 3 MB OTA slot).
- **Downloadable scripted apps** via an embedded MicroPython v1.28.0 VM. Scripts import a narrow `stick` module (display / buttons / imu / store / timing) and run in a sandboxed GC heap that's torn down after each launch.
- **On-device App Store** that fetches a server-hosted catalog over HTTPS, downloads files with SHA256 verification, and registers the app into the runtime registry without a reboot.
- **Firmware OTA** with dual 3 MB slots — `Settings → Update` checks a versioned manifest, streams the new binary into the inactive slot, verifies SHA256, and switches the boot target on success. LittleFS survives across updates so installed scripted apps persist.
- **FastAPI backend** that serves the catalog, firmware metadata, WiFi sync, and a WebSocket audio pipeline for the `scribe` transcription app. 17-test pytest suite covers every HTTP endpoint and the WebSocket protocol.

It's a dense slice of embedded infrastructure — persistent per-app NVS namespacing, a cooperative-exit PWR button, partitioned flash with OTA, a language runtime, a catalog fetcher, an HTTPS download pipeline — all in <1.6 MB of firmware.

## Apps

### Games (7)

| App | Description |
|---|---|
| **Flappy** | Side-scrolling pipe dodger with tap-to-flap controls |
| **Dino** | Chrome dino-style endless runner with jump and duck |
| **Scream** | Scream into the mic to fly higher — loudness controls altitude |
| **Galaxy** | Tilt-based space shooter, dodge asteroids and collect stars |
| **Balance** | Keep a ball centered on screen using the IMU accelerometer |
| **Simon** | Classic memory game — repeat the color/sound sequence |
| **Panic** | Fast-tap reflex game with escalating speed |

### Apps (3)

| App | Description |
|---|---|
| **Scribe** | Record audio over WebSocket for server-side Gemini transcription; **B** opens a history viewer for past transcripts (word-wrapped, scrollable with A/B) |
| **Timer** | Stopwatch with start/stop, lap counter, and centisecond precision |
| **Light** | Turns the display full white as a flashlight/torch |

### Sensors (4)

| App | Description |
|---|---|
| **Battery** | Real-time battery voltage, percentage, and charging status |
| **IMU** | Live accelerometer and gyroscope readings from the built-in MPU6886 |
| **Scan** | Scan and list nearby WiFi networks with signal strength and encryption |
| **Mic** | Real-time microphone level meter with peak detection |

### Settings (7)

| App | Description |
|---|---|
| **About** | Device info — firmware version, MAC address, flash/RAM usage |
| **WiFi** | View connected network, signal strength, and IP address |
| **Storage** | NVS, flash, heap, and LittleFS usage breakdown |
| **Apps** | List of all installed apps (native + scripted) with categories |
| **Time** | Current time display with manual NTP resync |
| **Update** | Check for firmware updates over HTTPS; dual-slot OTA apply |
| **Store** | Browse the catalog, download and install scripted apps |

### Downloadable scripted apps (MicroPython)

Shipped reference apps under `apps/`:

| App | Category | Description |
|---|---|---|
| **snake** | game | Grid snake. A turns right, B turns left |
| **tilt** | utility | IMU bubble level. A zeros the reference |

Scripted apps can only be `game` or `utility` — sensor/settings categories stay native.

> 📖 **[APPS.md](APPS.md)** — per-app reference with controls, persisted state, and hardware requirements for every app above.

## Repo structure

```
os/                  -> Stick OS firmware (Arduino sketch + partition table)
libraries/           -> Shared C++ libraries — one per app + OS core + MPY VM
apps/                -> MicroPython reference apps (.py + manifest.json)
server/              -> FastAPI backend (catalog, firmware, audio streaming)
  tests/             -> pytest suite (17 tests)
tools/               -> Build, flash, WiFi seed, MPY build, file/app push scripts
tools/user_c_modules/stick/  -> The Python `stick` module source (C + C++ bindings)
```

The MicroPython VM sources under `libraries/micropython_vm/` are generated — `./tools/build_mpy_vm.sh` clones MicroPython v1.28.0 on first run and extracts the embed-port output. The upstream clone is kept outside the tree.

## Quick start

### Build & flash

```bash
./tools/flash.sh os
```

The partition table at `os/partitions.csv` is auto-detected — dual OTA slots (3 MB each) + 1.9 MB LittleFS.

### Device configuration

```bash
cp libraries/stick_config/stick_config.h.example libraries/stick_config/stick_config.h
# Edit with your server host
```

### WiFi + API key

```bash
./tools/wifi_seed.py add --port /dev/cu.usbserial-XXXX --ssid "MyNetwork" --password "secret"
./tools/wifi_seed.py apikey-set --port /dev/cu.usbserial-XXXX --key "your-secret-key"
```

`WIFI_SET` uses a tab-delimited serial protocol internally, so SSIDs with spaces work.

### Server

```bash
cd server
pip install -r requirements.txt
cp .env.example .env  # GEMINI_API_KEY, STICK_API_KEY, STICK_DOMAIN
uvicorn main:app --host 0.0.0.0 --port 8765

# Run tests
pip install -r requirements-dev.txt
python3 -m pytest
```

Endpoints:
- `GET /api/catalog` — scripted-app catalog
- `GET /api/firmware` — firmware update manifest
- `GET /api/wifi` — WiFi credential sync
- `GET /api/transcripts` — list of past Scribe transcripts (`name`, `timestamp`, `size`)
- `GET /api/transcripts/<name>` — fetch a transcript body as plain text
- `GET /apps/<id>/<file>` / `GET /firmware/<file>` — static mounts for downloads
- `WS /services/scribe` — Scribe audio stream + Gemini transcription

## Installing scripted apps

Two paths:

**A) Dev: push from laptop over USB**
```bash
./tools/push_app.py --port /dev/cu.usbserial-XXXX --app apps/snake
```
Writes `/apps/snake/{manifest.json,main.py}` to the device's LittleFS via the `FILE_PUT` serial protocol. Reboot to pick up. `scanInstalledApps()` walks `/apps/` on every boot.

**B) User: on-device App Store**

1. Start the server and copy apps into server-served storage:
   ```bash
   cp -r apps/snake apps/tilt server/storage/apps/
   ```
2. On the device: **Settings → Store** → select app → **A** to install. Downloads each file from `/api/catalog` over HTTPS, verifies SHA256, writes to LittleFS, registers the descriptor immediately.

**Publishing to the server (dev flow):** `./tools/publish_app.py [snake tilt ...]` copies `apps/<id>/` into `server/storage/apps/<id>/` and regenerates `server/storage/catalog.json` with real sizes and sha256s. Run on the host that serves `/api/catalog`; restart uvicorn after a first-time publish into a previously-empty storage tree so `StaticFiles` mounts the new path.

Uninstall on device: Store → select installed app (green ✓) → **A** → confirm with **A**. Or `APP_RM <id>` over serial.

## Firmware OTA

**Settings → Update** fetches `/api/firmware`, compares against the baked-in version, downloads the new binary to the inactive OTA slot, verifies SHA256, marks the slot as boot target, reboots. LittleFS is untouched — installed scripted apps survive.

## The `stick.*` Python API (api_version 1)

Scripts import `stick` and use:

```python
stick.display.fill(color)
stick.display.rect(x, y, w, h, color)
stick.display.line(x0, y0, x1, y1, color)
stick.display.pixel(x, y, color)
stick.display.text(s, x, y, color)       # size 1
stick.display.text2(s, x, y, color)      # size 2
stick.display.width() / height()         # content rect (excludes status strip)

stick.buttons.update()                   # call each frame
stick.buttons.a_pressed() / b_pressed()

stick.imu.accel() -> (x, y, z)           # g
stick.imu.gyro()  -> (x, y, z)           # dps

stick.store.get(key, default) / put(key, value)   # per-app NVS

stick.millis() / stick.delay(ms)
stick.exit() -> True when PWR pressed

# Colors (RGB565)
stick.BLACK WHITE RED GREEN BLUE YELLOW CYAN
```

Extending: edit `tools/user_c_modules/stick/mod_stick.c` + `stick_bindings.cpp`, then `./tools/build_mpy_vm.sh` to regenerate.

## Editor tooling

`./tools/gen_clangd.sh` dumps arduino-cli's exact `-I`/`-D` flags into a `.clangd` at repo root so clangd can resolve `#include <M5StickCPlus2.h>` and friends and give useful diagnostics (instead of a wall of "file not found"). Regenerate after a core or library update. `.clangd` is gitignored — paths are user-specific.

## Hardware

[M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): ESP32-PICO-V3-02, 8 MB flash, built-in PDM mic, IR LED, 135×240 LCD, two buttons + power.

## Architecture

Each app is a self-contained library in `libraries/` that registers via `STICK_REGISTER_APP(...)`. The OS boots to a portrait-mode category picker (Games / Apps / Sensors / Settings). PWR-click exits any app. WiFi goes through `StickNet::` — apps never touch `WiFi.h` directly. The OS status strip (WiFi, battery, time) is drawn automatically for portrait-mode apps.

Scripted apps get a sandboxed-feeling view of the same hardware via the `stick` Python module. The runtime re-creates the MicroPython VM per-launch so a crashing script can't corrupt state beyond its own GC heap.

## License

MIT
