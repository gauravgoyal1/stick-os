# AiPin

Bluetooth audio streamer for the **M5StickC Plus 2**.

## Features

- **SPP slave mode** — the device advertises as "AiPin" and waits for your Mac to connect
- **Record & stream** audio from the built-in microphone over Bluetooth SPP
- **Real-time noise reduction** — high-pass filter and noise gate for cleaner audio
- **Icon-based UI** — visual interface with minimal text, easy to read at a glance
- **Save** streamed audio as `.wav` on your Mac via the included receiver script
- **Auto-detect** connection and disconnection with audio feedback

## Hardware

- [M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2) (ESP32-based)
- Built-in SPM1423 PDM microphone
- 240x135 LCD display
- Two buttons: BtnA (front), BtnB (side)

## Quick Start

### 1. Flash the firmware

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 -u -p /dev/cu.usbserial-XXXXX .
```

### 2. Pair with your Mac

1. Open **System Settings → Bluetooth** on your Mac
2. The device appears as **"AiPin"** — click Connect (PIN: `1234`)
3. A serial port `/dev/cu.AiPin` becomes available

### 3. Receive audio on Mac

```bash
pip install pyserial
python receiver.py --port /dev/cu.AiPin --output recording.wav
```

Running `receiver.py` opens the serial port, which triggers the SPP connection. The device detects the connection, shows the connected screen, and BtnA starts recording. The receiver captures streamed audio and writes a `.wav` file when recording stops.

### 4. Device buttons

| Screen | BtnA | BtnB |
|--------|------|------|
| Waiting | --- | --- |
| Connected | Record | Disconnect |
| Recording | Stop | Disconnect |

## Audio Streaming Protocol

Audio is streamed over Bluetooth SPP using a simple binary protocol:

| Packet | Size | Content |
|--------|------|---------|
| Start | 12 bytes | `APST` magic + sample rate (uint32) + bit depth (uint16) + channels (uint16) |
| Audio | 1024 bytes each | Raw PCM int16 chunks (512 samples) |
| Stop | 4 bytes | `APND` magic |

Default format: **16kHz, 16-bit, mono** (~32 KB/s).

## Audio Quality Features

The firmware includes real-time noise reduction processing:

- **High-pass filter** — removes DC offset and low-frequency rumble (< 100Hz)
- **Noise gate** — suppresses quiet background noise and hiss
- **Configurable thresholds** — adjust in `aipin.ino`:
  - `NOISE_GATE_THRESHOLD` (default: 150) — higher values = more aggressive noise suppression
  - `HPF_ALPHA` (default: 0.95) — filter coefficient for DC removal

The recording screen displays icons indicating:

- 🎤 Recording status with pulsing indicator
- 📶 Bluetooth SPP connection
- 🌊 Sample rate (16kHz)
- 🔽 Noise reduction (NR) active

## Receiver Options

```
python receiver.py --list                  # List serial ports
python receiver.py -p PORT                 # Auto-timestamped output
python receiver.py -p PORT -o file.wav     # Named output
python receiver.py -p PORT --continuous    # Multiple recordings
python receiver.py -p PORT --verbose       # Debug logging
```

## Requirements

**Firmware:**
- `arduino-cli` with M5Stack ESP32 board package
- Libraries: M5StickCPlus2, BluetoothSerial (ESP32 core)

**Receiver:**
- Python 3
- `pyserial` (`pip install pyserial`)

## Notes

- The device runs in BT slave mode — it does not scan or initiate connections
- The Mac connects by opening `/dev/cu.AiPin` (e.g. via `receiver.py`)
- Speaker and microphone share GPIO 0 and cannot run simultaneously — speaker is automatically disabled during recording
- Serial debug logging is available at 115200 baud over USB
