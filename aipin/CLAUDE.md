# CLAUDE.md — AiPin sub-project

AiPin is an audio-capture device built on the M5StickC Plus 2. Two firmware variants (`bl/` and `wifi/`) stream audio to a host using the same APST/APND protocol; the matching host-side receiver lives in `server/` (Python, WiFi variant) and `bl/receiver.py` (Python, BT variant).

## Sub-dirs

- `bl/` — Bluetooth SPP audio streamer. Runs as a BT slave ("AiPin"), Mac connects by opening `/dev/cu.AiPin`. `bl/receiver.py` is the matching host-side receiver. Primary sketch: `bl.ino`.
- `wifi/` — WiFi + TCP audio streamer. Connects to a hardcoded known WiFi network and streams to a TCP server. Primary sketch: `wifi.ino`. **Update WiFi credentials and server IP in `wifi.ino` before deploying.**
- `server/` — Python TCP server. Receives WiFi audio streams, saves `.wav` files, optionally transcribes with Gemini AI (speaker diarization). Requires `GEMINI_API_KEY` in `.env` at the repo root.

## Build & Upload

```bash
./tools/flash.sh aipin/bl    # compile + upload the BT variant
./tools/flash.sh aipin/wifi  # compile + upload the WiFi variant
```

For direct compile without upload:
```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 --libraries "$(git rev-parse --show-toplevel)/libraries" aipin/bl
```

## Audio streaming protocol (shared by both variants)

12-byte `APST` header (magic + sample rate uint32 + bit depth uint16 + channels uint16), then raw PCM chunks (1024 bytes each), then 4-byte `APND` stop marker.

## Audio format

8 kHz 8-bit mono. The mic captures 16-bit natively; firmware applies a processing pipeline (gain, HPF, LPF, noise gate, soft/hard clipping) and downsamples to 8-bit before transmission.

## UI pattern

All screens follow `drawHeader()` / content / `drawFooter()` structure. Header shows title + battery %. Footer shows BtnA/BtnB action hints. Display: 135×240 portrait (rotation 0). Inputs: BtnA (front), BtnB (side).

## Key constraints

- **Speaker and microphone share GPIO 0.** `Speaker.end()` must be called before `Mic.begin()` and vice versa.
- Colors are initialized via `initColors()` after display init — use the `C_*` globals, not raw color constants.
- Audio processing parameters are tunable at runtime via Serial commands: `gain`, `gate`, `hpf`, `lpf`, `knee`, `ratio`, `audio`.
- WiFi variant: credentials and server IP are hardcoded in `wifi.ino` — update before deploying.

## Host-side receivers

```bash
pip install pyserial
python aipin/bl/receiver.py --port /dev/cu.AiPin

pip install requests
python aipin/server/server.py --port 8765
```
