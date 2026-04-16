# stick-os

Launcher firmware for the **M5StickC Plus 2** (ESP32) — a tiny wearable computer with a portrait-mode category picker, 19 native apps, WiFi connectivity, and a FastAPI backend for app catalog and audio streaming.

```
 ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
 │ Stick  ▂▄▆█ 4:25│  │ Apps   ▂▄▆█ 4:25│  │ Scribe ▂▄▆█ 4:25│  │ Scribe ▂▄▆█ 4:25│
 │─────────────────│  │─────────────────│  │─────────────────│  │─────────────────│
 │                 │  │                 │  │                 │  │                 │
 │  ┌─────────────┐│  │  ┌─────────────┐│  │                 │  │     01:23       │
 │  │ ● Games    7││  │  │ ● Scribe    ││  │      ████       │  │                 │
 │  └─────────────┘│  │  └─────────────┘│  │     ██✓██       │  │                 │
 │  ┏━━━━━━━━━━━━━┓│  │  ┌─────────────┐│  │      ████       │  │  ▌▌▐█▌▐▌█▐█▌▌  │
 │  ┃ ● Apps     3┃│  │  │ ● Timer     ││  │                 │  │ ▌█▌██▐█▌██▐██▌▌ │
 │  ┗━━━━━━━━━━━━━┛│  │  └─────────────┘│  │     Ready       │  │▐█▐▐██▐█▐███▐██▐▌│
 │  ┌─────────────┐│  │  ┌─────────────┐│  │                 │  │ ▌█▌██▐█▌██▐██▌▌ │
 │  │ ● Sensors  4││  │  │ ● Light     ││  │ Press to Record │  │  ▌▌▐█▌▐▌█▐█▌▌  │
 │  └─────────────┘│  │  └─────────────┘│  │                 │  │                 │
 │  ┌─────────────┐│  │                 │  │   REDACTED-WIFI    │  │                 │
 │  │ ● Settings 5││  │                 │  │     ▂▄▆█        │  │                 │
 │  └─────────────┘│  │                 │  │                 │  │                 │
 │                 │  │                 │  │                 │  │                 │
 └─────────────────┘  └─────────────────┘  └─────────────────┘  └─────────────────┘
   Category picker       App list          Scribe: ready        Scribe: recording
```

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
| **Scribe** | Record audio and stream to the server over WebSocket for AI transcription via Gemini. Live waveform visualization while recording |
| **Timer** | Stopwatch with start/stop, lap counter, and centisecond precision |
| **Light** | Turns the display full white as a flashlight/torch |

### Sensors (4)

| App | Description |
|---|---|
| **Battery** | Real-time battery voltage, percentage, and charging status |
| **IMU** | Live accelerometer and gyroscope readings from the built-in MPU6886 |
| **Scan** | Scan and list nearby WiFi networks with signal strength and encryption |
| **Mic** | Real-time microphone level meter with peak detection |

### Settings (5)

| App | Description |
|---|---|
| **About** | Device info — firmware version, MAC address, flash/RAM usage |
| **WiFi** | View connected network, signal strength, and IP address |
| **Storage** | NVS and flash storage usage breakdown |
| **Apps** | List of all installed apps with categories and version info |
| **Time** | Current time display with manual NTP resync |

## Repo structure

```
os/          -> Stick OS firmware (Arduino sketch)
libraries/   -> Shared C++ libraries (one per app + OS core)
server/      -> FastAPI backend (catalog, firmware updates, audio streaming)
tools/       -> Build, flash, and WiFi seeding scripts
apps/        -> MicroPython app sources (Phase 2, not yet active)
docs/        -> Design specs and mockups
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
cp .env.example .env  # fill in GEMINI_API_KEY, STICK_API_KEY, STICK_DOMAIN
uvicorn main:app --host 0.0.0.0 --port 8765
```

## Hardware

[M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): ESP32-PICO-V3-02, 8 MB flash, built-in PDM mic, IR LED, 135x240 LCD, two buttons.

## Architecture

Each app is a self-contained library in `libraries/` that registers via `STICK_REGISTER_APP(...)`. The OS boots to a portrait-mode category picker (Games / Apps / Sensors / Settings). PWR-click exits any app. WiFi goes through `StickNet::` — apps never touch `WiFi.h` directly. The OS status strip (WiFi, battery, time) is drawn automatically for portrait-mode apps.

## License

MIT
