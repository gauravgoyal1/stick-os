# AiPin

Bluetooth device scanner, connection manager, and audio streamer for the **M5StickC Plus 2**.

## Features

- **Scan** for Classic Bluetooth devices (5-second discovery)
- **Browse** results in a scrollable list sorted by signal strength
- **Connect** to any device via SPP (Serial Port Profile)
- **Auto-reconnect** to the last connected device on boot and after connection loss
- **Record & stream** audio from the built-in microphone to the connected device
- **Save** streamed audio as `.wav` on your Mac

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

### 2. Use the device

| Screen | BtnA | BtnB |
|--------|------|------|
| Reconnecting | Skip to Scan | --- |
| Scan Results | Connect | Next device |
| Connected | Record | Disconnect |
| Recording | Stop | Disconnect |

### 3. Receive audio on Mac

```bash
pip install pyserial
python receiver.py --port /dev/cu.AiPin --output recording.wav
```

The receiver waits for the AiPin to start recording, captures the streamed audio, and writes a `.wav` file when recording stops.

## Audio Streaming Protocol

Audio is streamed over Bluetooth SPP using a simple binary protocol:

| Packet | Size | Content |
|--------|------|---------|
| Start | 12 bytes | `APST` magic + sample rate (uint32) + bit depth (uint16) + channels (uint16) |
| Audio | 1024 bytes each | Raw PCM int16 chunks (512 samples) |
| Stop | 4 bytes | `APND` magic |

Default format: **16kHz, 16-bit, mono** (~32 KB/s).

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

- macOS devices only appear in BT scan when their Bluetooth Settings panel is open
- Speaker and microphone share GPIO 0 and cannot run simultaneously -- speaker is automatically disabled during recording
