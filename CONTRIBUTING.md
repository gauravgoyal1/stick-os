# Contributing to stick-os

Welcome. This project has a few moving pieces — firmware (Arduino C++), a MicroPython runtime, a FastAPI backend, and Python tooling. Most changes are self-contained to one of them.

## Layout orientation

| You're touching | Look here |
|---|---|
| Adding a native app | `libraries/<app_id>/` — follow the pattern in `libraries/app_flashlight/` or `libraries/game_simon/` |
| Changing the launcher | `os/launcher_*` and `os/os.ino` |
| Adding a `stick.*` Python binding | `tools/user_c_modules/stick/mod_stick.c` + `stick_bindings.cpp`, then `./tools/build_mpy_vm.sh` |
| Shipping a scripted app | `apps/<id>/manifest.json` + `main.py`, install via `tools/push_app.py` or the Store |
| Server endpoint or test | `server/api/`, `server/services/`, `server/tests/` |

## Before you start coding

1. Copy `libraries/stick_config/stick_config.h.example` → `stick_config.h` and fill in your server host.
2. Seed WiFi + API key: `./tools/wifi_seed.py add …` and `apikey-set`.
3. Confirm the baseline build flashes and boots: `./tools/flash.sh os`.

## Firmware changes

**Build:**
```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 \
  --libraries "$(git rev-parse --show-toplevel)/libraries" os
```

**Size discipline:** The firmware slot is 3 MB. Check the "Sketch uses N bytes" line in compile output after every change. Features adding > 50 KB need explicit justification — call it out in the PR description.

**New app checklist:**
- Library dir under `libraries/<id>/`
- `<id>.h` with `init()`, `tick()`, `icon(x, y, color)` declarations
- `<id>.cpp` implementing the namespace + a static `AppDescriptor` registered via `STICK_REGISTER_APP(...)`
- `#include <id.h>` added to `os/os.ino`
- Apps respect the status strip (draw below `stick_os::kStatusStripHeight` in portrait)
- Apps exit via `stick_os::checkAppExit()` (or `ArcadeCommon::updateAndCheckExit()` in games)

## Adding a `stick.*` Python binding

1. Declare the extern-C signature in `tools/user_c_modules/stick/stick_bindings.h`.
2. Implement it in `stick_bindings.cpp`.
3. Add the Python wrapper in `mod_stick.c` — `MP_DEFINE_CONST_FUN_OBJ_*` + an entry in the appropriate submodule's `globals_table`.
4. Regenerate: `./tools/build_mpy_vm.sh` (this clones MicroPython on first run, then just re-runs QSTR extraction + copies sources).
5. Rebuild firmware, verify on-device with `MPY_RUN <path>` over serial.

Every symbol you add is a **forward-compatibility promise** — the `apps/` scripts depend on the surface staying stable. Renaming or removing functions is a breaking change.

## Scripted apps

Restricted to `CAT_GAME` or `CAT_UTILITY` only. Sensor and settings apps stay native because they need OS-internal access (hardware, NVS, OTA). The registry enforces this at `registerApp()` time.

## Server changes

```bash
cd server
pip install -r requirements.txt -r requirements-dev.txt
uvicorn main:app --host 0.0.0.0 --port 8765
python3 -m pytest        # 17 tests, ~0.3s
```

New endpoints need a test under `server/tests/`. The `client` fixture in `conftest.py` points STORAGE at a tmp dir so tests never touch real server data.

## Commit style

- Subject: short imperative — `feat(2c): add stick.imu bindings`, `fix: off-by-one in launcher scroll`
- Body wraps at ~72 chars
- No `Co-Authored-By` lines
- For Phase 2 work, prefix with the task ID: `feat(2c)`, `feat(2d)`, etc.

## Hardware testing

Flash, watch serial, click through the launcher. The automated tests only cover the server — all firmware verification is empirical. Before merging anything that changes boot, launcher nav, or OTA, flash and confirm:
- Device boots cleanly (WiFi connects, LittleFS mounts, no crashes in serial log)
- All four categories still open
- At least one native app launches and exits cleanly via PWR

## Reporting issues

File in the [issue tracker](https://github.com/gauravgoyal1/stick-os/issues). Please include:
- Firmware commit SHA (see `[stick]` boot log line)
- Hardware revision of your M5StickC Plus 2
- Relevant serial output (use `arduino-cli monitor --port <port> --config baudrate=115200`)
