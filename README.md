# stick-os

Launcher firmware for the **M5StickC Plus 2** (ESP32) — a tiny wearable computer with a portrait-mode category picker, 19 native apps, WiFi connectivity, and a FastAPI backend for app catalog and audio streaming.

## What's inside

| Category | Apps |
|---|---|
| **Games (7)** | Flappy, Dino, Scream, Galaxy, Balance, Simon, Panic |
| **Apps (3)** | AiPin (audio streaming), Stopwatch, Flashlight |
| **Sensors (4)** | Battery, IMU, WiFi Scan, Mic Meter |
| **Settings (5)** | About, WiFi, Storage, Installed Apps, Time |

## Repo structure

```
os/          → Stick OS firmware (Arduino sketch)
libraries/   → Shared C++ libraries (one per app + OS core)
server/      → FastAPI backend (catalog, firmware updates, audio streaming)
tools/       → Build, flash, and WiFi seeding scripts
apps/        → MicroPython app sources (Phase 2, not yet active)
docs/        → Design specs and implementation plans
```

## Quick start

### Build & flash

```bash
# Compile + upload the OS firmware
./tools/flash.sh os

# Compile only (no upload)
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" os
```

### Device configuration

```bash
cp libraries/stick_config/stick_config.h.example libraries/stick_config/stick_config.h
# Edit with your server host
```

### WiFi setup

WiFi credentials are stored in NVS (device flash), never in source code.

```bash
./tools/wifi_seed.py add --port /dev/cu.usbserial-XXXX --ssid "MyNetwork" --password "secret"
./tools/wifi_seed.py list --port /dev/cu.usbserial-XXXX
./tools/wifi_seed.py delete --port /dev/cu.usbserial-XXXX --ssid "MyNetwork"
```

### API key

Services require a shared API key stored in NVS and validated server-side.

```bash
./tools/wifi_seed.py apikey-set --port /dev/cu.usbserial-XXXX --key "your-secret-key"
./tools/wifi_seed.py apikey-get --port /dev/cu.usbserial-XXXX
```

### Server

```bash
cd server
pip install -r requirements.txt
cp .env.example .env  # fill in GEMINI_API_KEY, STICK_DOMAIN
uvicorn main:app --host 0.0.0.0 --port 8765
```

## Hardware

[M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): ESP32-PICO-V3-02, 8 MB flash, built-in PDM mic, IR LED, 135x240 LCD, two buttons.

## Architecture

Each app is a self-contained library in `libraries/` that registers via `STICK_REGISTER_APP(...)`. The OS boots to a portrait-mode category picker (Games / Apps / Sensors / Settings). PWR-click exits any app. WiFi goes through `StickNet::` — apps never touch `WiFi.h` directly.

## License

MIT
