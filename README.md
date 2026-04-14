# electronics-lab

A personal sandbox for small electronics projects, mostly on the **M5StickC Plus 2** (ESP32), plus breadboard / sensor experiments over time.

Each sub-project is self-contained and can be built independently. Shared tooling lives at the repo root.

## Projects

| Dir | What it is | Status |
|---|---|---|
| [`aipin_bl/`](aipin_bl) | AiPin — Bluetooth SPP audio streamer | Working |
| [`aipin_wifi/`](aipin_wifi) | AiPin — WiFi TCP audio streamer | Working |
| [`server/`](server) | Python TCP server + Gemini transcription for AiPin WiFi | Working |
| [`clap_remote/`](clap_remote) | Clap-activated IR remote for a TCL Mini LED TV | 🚧 Phase 1 in progress |

## Shared tooling

- [`tools/flash.sh`](tools/flash.sh) — one-liner to `arduino-cli compile && upload` any sketch directory. Hard-codes the M5StickC Plus 2 FQBN and a USB port; edit those two lines if you use a different board or port.

## Hardware

Most projects target the [M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): ESP32-PICO-V3-02, built-in PDM mic, IR LED, 135×240 LCD, two buttons. Future projects may branch out to a bare ESP32 + breadboard + sensors.

## Convention

- Firmware directories contain one Arduino sketch each (`<name>/<name>.ino`), with optional `.h`/`.cpp` siblings that arduino-cli picks up automatically.
- Per-project docs live inside the project folder.
- Root-level `CLAUDE.md` gives working notes to Claude / future-me for the whole repo.

## Quick reference — build any sketch

```bash
./tools/flash.sh <project>/firmware/<sketch>
# or, for the flat AiPin sketches:
./tools/flash.sh aipin_bl
./tools/flash.sh aipin_wifi
```

See each sub-project's README for project-specific setup (WiFi credentials, pairing, etc.).
