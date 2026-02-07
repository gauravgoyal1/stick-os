# AiPin

Audio streamer for the **M5StickC Plus 2** with Bluetooth and WiFi transport options.

## Features

- **Two transport modes** — Bluetooth SPP or WiFi TCP streaming
- **Record & stream** audio from the built-in microphone
- **Real-time audio processing** — gain, high-pass filter, low-pass filter, noise gate, soft clipping
- **Icon-based UI** — visual interface with battery, signal strength, and recording indicators
- **Gemini AI transcription** — automatic speech-to-text with speaker diarization (WiFi mode)
- **Auto-reconnect** — recovers from WiFi/server/BT disconnections

## Hardware

- [M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2) (ESP32-based)
- Built-in SPM1423 PDM microphone
- 135x240 LCD display
- Two buttons: BtnA (front), BtnB (side)

## Project Structure

```
aipin_bl/          # Bluetooth variant
  aipin.ino        #   Firmware (BT SPP slave mode)
  receiver.py      #   Mac-side receiver + transcription
aipin_wifi/        # WiFi variant
  aipin_wifi.ino   #   Firmware (WiFi + TCP client)
server/            # WiFi server
  server.py        #   TCP server + Gemini transcription
  README.md        #   Server-specific docs
```

## Quick Start — Bluetooth Mode

### 1. Flash the firmware

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 -u -p /dev/cu.usbserial-XXXXX aipin_bl/
```

### 2. Pair with your Mac

1. Open **System Settings > Bluetooth** on your Mac
2. The device appears as **"AiPin"** — click Connect (PIN: `1234`)
3. A serial port `/dev/cu.AiPin` becomes available

### 3. Receive audio

```bash
pip install pyserial
python aipin_bl/receiver.py --port /dev/cu.AiPin
```

## Quick Start — WiFi Mode

### 1. Configure WiFi and server

Edit `aipin_wifi/aipin_wifi.ino` — update WiFi credentials and server IP address.

### 2. Flash the firmware

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 -u -p /dev/cu.usbserial-XXXXX aipin_wifi/
```

### 3. Start the server

```bash
pip install requests
python server/server.py --port 8765
```

The server prints its local IP — use that in the firmware config. Recordings are saved to `recordings/` and transcripts to `transcripts/`.

### 4. Transcription

Set `GEMINI_API_KEY` in a `.env` file at the project root. The server automatically transcribes recordings with speaker diarization.

## Device Buttons

| Screen | BtnA | BtnB |
|--------|------|------|
| Waiting | --- | --- |
| Connected | Record | Disconnect |
| Recording | Stop | Disconnect |

## Audio Streaming Protocol

Audio is streamed using a simple binary protocol (same for both BT and WiFi):

| Packet | Size | Content |
|--------|------|---------|
| Start | 12 bytes | `APST` magic + sample rate (uint32) + bit depth (uint16) + channels (uint16) |
| Audio | 1024 bytes each | Raw PCM chunks |
| Stop | 4 bytes | `APND` magic |

Default format: **8kHz, 8-bit, mono** (~8 KB/s).

## Audio Processing Pipeline

The firmware processes audio in real-time before streaming:

1. **Gain** — software amplification
2. **High-pass filter** — removes DC offset and low-frequency rumble
3. **Low-pass filter** — removes high-frequency noise
4. **Noise gate** — suppresses quiet background noise
5. **Soft clipping** — compresses peaks for natural limiting
6. **Hard clipping** — prevents overflow

All parameters are tunable at runtime via Serial commands: `gain`, `gate`, `hpf`, `lpf`, `knee`, `ratio`.

## Requirements

**Firmware:**
- `arduino-cli` with M5Stack ESP32 board package
- Libraries: M5StickCPlus2

**Bluetooth receiver:**
- Python 3, `pyserial`

**WiFi server:**
- Python 3, `requests`
- `GEMINI_API_KEY` (optional, for transcription)

## Notes

- Speaker and microphone share GPIO 0 — speaker is automatically disabled during recording
- Serial debug logging available at 115200 baud over USB
- WiFi variant auto-reconnects to WiFi and server on connection loss
- BT variant runs in slave mode — the Mac initiates the connection
