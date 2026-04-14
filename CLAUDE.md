# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repo overview

`electronics-lab` is a sandbox for small electronics / M5StickC Plus 2 / ESP32 projects. Each top-level directory is an independent sub-project. Sub-projects may share some tooling (see `tools/flash.sh`) but otherwise don't depend on each other.

Current sub-projects:

- **`aipin_bl/`** — AiPin Bluetooth SPP audio streamer. Runs as a BT slave ("AiPin"), Mac connects via serial port.
- **`aipin_wifi/`** — AiPin WiFi + TCP audio streamer. Connects to a known WiFi network and streams to a TCP server.
- **`server/`** — Python TCP server that receives WiFi audio streams, saves WAV files, and transcribes with Gemini AI.
- **`clap_remote/`** — Clap-activated IR remote for a TCL Mini LED TV. Phase 1 in progress; see `clap_remote/CLAUDE.md` and `clap_remote/docs/superpowers/` for the full spec, plan, and working notes.

## Build & Upload Commands

Uses `arduino-cli` with the M5Stack board package.

```bash
# Compile (specify sketch directory)
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 aipin_bl/
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 aipin_wifi/

# Find connected device port
arduino-cli board list

# Upload (replace port as needed)
arduino-cli upload --fqbn m5stack:esp32:m5stack_stickc_plus2 -p /dev/cu.usbserial-XXXXX aipin_wifi/

# Compile + upload in one shot
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 -u -p /dev/cu.usbserial-XXXXX aipin_wifi/
```

**Board package sources** (configured in arduino-cli):
- ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
- M5Stack: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

**Required libraries**: M5StickCPlus2, BluetoothSerial (ESP32 core, BT variant only), WiFi (ESP32 core, WiFi variant only)

## Architecture

### Bluetooth variant (`aipin_bl/aipin.ino`)

Single-file state machine. Runs in BT slave mode, advertising as "AiPin". Mac connects by opening `/dev/cu.AiPin` (e.g. via `receiver.py`). Connected screen shows device info; BtnA starts recording, BtnB disconnects.

### WiFi variant (`aipin_wifi/aipin_wifi.ino`)

Single-file state machine. On boot, scans for known WiFi networks (hardcoded credentials), connects, then establishes a TCP connection to the server. Auto-reconnects on WiFi or server loss.

### Server (`server/server.py`)

TCP server that accepts connections from WiFi-mode AiPin devices. Handles multiple clients via threads. Receives audio using the same APST/APND protocol, saves WAV files, and optionally transcribes with Gemini API (speaker diarization).

### Shared architecture

**Audio streaming protocol**: 12-byte `APST` header (magic + sample rate uint32 + bit depth uint16 + channels uint16), then raw PCM chunks (1024 bytes each), then 4-byte `APND` stop marker.

**Audio format**: 8kHz 8-bit mono. Mic captures 16-bit natively; firmware applies a processing pipeline (gain, HPF, LPF, noise gate, soft/hard clipping) then downsamples to 8-bit.

**UI pattern**: All screens follow `drawHeader()` / content / `drawFooter()` structure. Header shows title + battery %. Footer shows BtnA/BtnB action hints.

**Display**: 135x240 pixels, portrait (rotation 0). **Inputs**: BtnA (front button), BtnB (side button).

## Key Constraints

- Colors are initialized via `initColors()` after display init — use the `C_*` globals, not raw color constants
- **Speaker and microphone share GPIO 0** — `Speaker.end()` must be called before `Mic.begin()` and vice versa
- Audio processing parameters are tunable at runtime via Serial commands: `gain`, `gate`, `hpf`, `lpf`, `knee`, `ratio`, `audio`
- WiFi variant: credentials and server IP are hardcoded in `aipin_wifi.ino` — update before deploying
- Server requires `GEMINI_API_KEY` in `.env` (project root) for transcription

## Bluetooth Receiver

`aipin_bl/receiver.py` runs on the Mac to receive BT audio and save as `.wav` files.

```bash
pip install pyserial
python aipin_bl/receiver.py --port /dev/cu.AiPin --continuous
```

## WiFi Server

`server/server.py` runs on the Mac/server to receive WiFi audio.

```bash
pip install requests
python server/server.py --port 8765
```
