# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AiPin is a Bluetooth device scanner, connection manager, and **audio streamer** for the **M5StickC Plus 2** (ESP32-based microcontroller). It discovers Classic Bluetooth devices, displays them in a scrollable list, connects via SPP, and can record audio from the built-in microphone and stream it to the connected device.

## Build & Upload Commands

Uses `arduino-cli` with the M5Stack board package.

```bash
# Compile
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 .

# Find connected device port
arduino-cli board list

# Upload (replace port as needed)
arduino-cli upload --fqbn m5stack:esp32:m5stack_stickc_plus2 -p /dev/cu.usbserial-XXXXX .

# Compile + upload in one shot
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 -u -p /dev/cu.usbserial-XXXXX .
```

**Board package sources** (configured in arduino-cli):
- ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
- M5Stack: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

**Required libraries**: M5StickCPlus2, BluetoothSerial (ESP32 core)

## Architecture

Single-file state machine in `aipin.ino` with four states:

```
STATE_SCANNING → STATE_SCAN_RESULTS → STATE_CONNECTING → STATE_CONNECTED
                      ↑                                        |
                      └──────── disconnect / lost connection ───┘
```

**Scan flow**: Classic BT discovery (5s blocking via `SerialBT.discover()`). Results stored in `devices[]` array, sorted named-first then by RSSI descending.

**Connection flow**: Classic BT connects via SPP (`SerialBT.connect(addr)`). Connected screen shows device name, MAC address, device class, and RSSI. BtnA starts recording, BtnB disconnects.

**Audio streaming flow**: When recording, the built-in SPM1423 PDM microphone captures 16kHz 16-bit mono audio. Raw PCM data is streamed over SPP using a simple protocol: 12-byte `APST` header (magic + format), then raw PCM chunks (1024 bytes each), then 4-byte `APND` stop marker. The `isRecording` flag extends `STATE_CONNECTED` without adding a new state.

**UI pattern**: All screens follow `drawHeader()` / content / `drawFooter()` structure. The header shows title + battery %. The footer shows BtnA/BtnB action hints. Device list supports scrolling with `scrollOffset` / `selectedIndex`.

**Display**: 240x135 pixels, landscape (rotation 1). **Inputs**: BtnA (front button), BtnB (side button).

## Key Constraints

- Max 20 scanned devices (`MAX_DEVICES`)
- 6 visible items per screen (`VISIBLE_ITEMS`), scrollable via `scrollOffset`
- `BluetoothSerial::begin("AiPin", true)` — second arg `true` enables master mode (required for scanning/connecting)
- macOS devices only appear in Classic BT scan when their Bluetooth settings panel is open
- Colors are initialized via `initColors()` after display init — use the `C_*` globals, not raw color constants
- **Speaker and microphone share GPIO 0** — `Speaker.end()` must be called before `Mic.begin()` and vice versa
- Audio streaming: 16kHz 16-bit mono PCM over SPP (~32KB/s, well within SPP bandwidth)

## Python Audio Receiver

`receiver.py` runs on the Mac to save streamed audio as `.wav` files.

```bash
pip install pyserial

# List available serial ports
python receiver.py --list

# Receive a single recording
python receiver.py --port /dev/cu.AiPin --output recording.wav

# Auto-timestamped filename
python receiver.py --port /dev/cu.AiPin

# Continuous mode (multiple recordings)
python receiver.py --port /dev/cu.AiPin --continuous
```
