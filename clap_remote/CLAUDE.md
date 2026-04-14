# CLAUDE.md — clap_remote sub-project notes

Living notes for the assistant. Update as the project evolves.

## Project

Clap-activated IR remote on M5StickC Plus 2. Phase 2 will add Wi-Fi + voice.

## Environment (verified 2026-04-14)

- macOS, zsh
- `arduino-cli` at `/opt/homebrew/bin/arduino-cli`
- Cores installed: `esp32:esp32@3.3.6`, `m5stack:esp32@3.2.5`
- Libraries installed: `M5Unified`, `M5StickCPlus2`, `IRremote`, `arduinoFFT`, `M5GFX`, `M5HAL`, `M5Utility`
- Device port: `/dev/cu.usbserial-5B1E0428761` (Board name reported as Unknown — normal for M5StickC Plus 2)

## Hardware references

- M5StickC Plus 2 — ESP32-PICO-V3-02, 8MB flash, 2MB PSRAM
- IR LED: built-in. **Do not hard-code the GPIO** — read it from M5Unified board headers at compile time; the Plus 2 pinout differs from the original M5StickC and we don't want magic numbers drifting from the truth.
- PDM Mic: built-in. Same rule — use the M5Unified `Mic` API, don't name GPIO pins directly.
- Buttons: use M5Unified's `M5.BtnA` / `M5.BtnB` / `M5.BtnPWR` API (debounce and edge detection are built in).
- Buzzer: available via M5Unified (not used in Phase 1).
- LCD: 135×240 ST7789v2, driven by `M5.Display`.

## FQBN

Use `m5stack:esp32:m5stack_stickc_plus2`.

## Conventions

- The main clap_remote sketch lives flat at `clap_remote/clap_remote.ino` (plus helper `.h`/`.cpp` files alongside it). No nested `firmware/` layer.
- `clap_remote/ir_codes.h` holds the TCL NEC power-toggle codes. arduino-cli picks it up automatically because it sits in the sketch directory.
- Project-agnostic diagnostic sketches (`hello_world`, `ir_probe`, `ir_sweep`) live at the repo top level under `diagnostics/`, not inside clap_remote — they've been promoted so any future project can reuse them.
- Specs live under `clap_remote/docs/superpowers/specs/YYYY-MM-DD-<topic>-design.md`.
- Plans live under `clap_remote/docs/superpowers/plans/` but are gitignored — they're session-scoped workflow artifacts, not tracked deliverables.
- Build/upload via `./tools/flash.sh clap_remote` from the repo root (or any sketch dir path).

## Phase 1 decisions (locked in brainstorm, 2026-04-14)

- Target: TCL Mini LED TV, NEC protocol, code discovered by `diagnostics/ir_probe` cycling through known candidates.
- Gesture: 2 claps → power toggle. 3-clap gesture dropped.
- Feedback: LCD + LED flash. No buzzer.
- Power: USB, permanent, near the TV.
- Detection: strict — envelope onset + FFT spectral gate (2–5 kHz band, ≥55 % in-band).
- Pair window: 150–600 ms between claps.
- Safety: Button A ARM/DISARM, 10 s post-fire cooldown, cooldown independent of disarm.
- Architecture: layered sketch (`mic` / `detector` / `gesture` / `ir` / `ui`) so Phase 2 can replace only `detector`.

See full spec at `clap_remote/docs/superpowers/specs/2026-04-14-clap-ir-remote-design.md`.

## Open questions (to resolve during build)

- Confirm actual IR LED GPIO from M5Unified headers on first compile. (Mid-Task-2 investigation: the pin-sweep result is still outstanding; see the plan file for context.)
- Confirm which TCL NEC power code works for the user's specific Mini LED model.
