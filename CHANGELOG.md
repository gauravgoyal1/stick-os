# Changelog

Notable changes to stick-os. Phases follow the architecture roadmap: 0 = OS/app isolation foundation, 1 = sensors/settings/tools, 2 = scripted apps + OTA + App Store.

## Phase 2 — scripted apps, OTA, App Store (current)

**2h — Firmware OTA.** `libraries/stick_os/stick_ota.h/.cpp` fetches `/api/firmware`, streams the binary into the inactive OTA slot, verifies SHA256, and marks the slot as boot target. `libraries/settings_update/` adds the `Settings → Update` UI. LittleFS is untouched so installed scripted apps survive.

**2g — App Store.** `libraries/app_store/` adds a `Settings → Store` native app that parses `/api/catalog`, downloads each file with streaming SHA256 verification, writes to LittleFS, and calls `registerInstalledApp()`. New app installs without a reboot.

**2e — .stickapp installer.** `libraries/stick_os/app_installer.h/.cpp` walks `/apps/*/manifest.json` on boot and registers every scripted app into the runtime registry. `uninstallApp(id)` tears down the descriptor and recursively deletes the directory. `APP_RM <id>` serial command.

**2d — ScriptHost + launcher wiring.** `libraries/stick_os/script_host.h/.cpp` reads a `.py` file from LittleFS, inits the MPY VM, executes the source, tears down. `enterApp()` dispatches `RUNTIME_MPY` descriptors through ScriptHost. New dev serial commands: `FILE_PUT`, `FILE_LS`, `FILE_RM`, `MPY_RUN`. Scripted apps are restricted to `CAT_GAME` or `CAT_UTILITY` at the registry level.

**2c — `stick.*` Python bindings.** `tools/user_c_modules/stick/` holds the canonical `mod_stick.c` + C++ bindings. Full api_version 1 surface: `display.fill/rect/line/pixel/text/text2/width/height`, `buttons.update/a_pressed/b_pressed`, `imu.accel/gyro`, `store.get/put`, `millis/delay/exit`, and the 7 RGB565 color constants.

**2b — MicroPython VM embed.** `libraries/micropython_vm/` is generated from MicroPython v1.28.0 by `tools/build_mpy_vm.sh`. Measurement gate passed: +126 KB flash, 204 KB free heap after init. xtensa uses setjmp-based NLR and GC-regs fallbacks.

**2a — Partition table + LittleFS + dynamic registry.** `os/partitions.csv` (dual 3 MB OTA + 1.9 MB LittleFS). `stick_os::fsInit()` mounts LittleFS on boot. `registerApp()` and `unregisterApp()` extend the static `STICK_REGISTER_APP` pipeline with runtime registration.

### Tooling

- `tools/build_mpy_vm.sh` — clones MicroPython, regenerates embed sources with QSTR extraction for the `stick` module.
- `tools/push_file.py` / `push_app.py` — push files or whole app directories over USB serial.
- Server test suite: 17 tests covering all HTTP endpoints + the Scribe WebSocket protocol.

### Rename

- `aipin_wifi_app` → `scribe` (client lib + server endpoint + recording file prefixes).
- `WIFI_SET` serial protocol switched from space to tab delimiter so SSIDs with spaces work.

### Reference scripted apps

- `apps/snake/` — classic grid snake, A=right, B=left, best-score in `stick.store`.
- `apps/tilt/` — IMU bubble level.
- `apps/demo/` — dev smoke-test.

## Phase 1 — sensors, settings, app ecosystem

- Added `sensor_battery`, `sensor_imu`, `sensor_wifi_scan`, `sensor_mic`.
- Added `settings_about`, `settings_wifi`, `settings_storage`, `settings_apps`, `settings_time`.
- Added `app_stopwatch`, `app_flashlight`.
- WiFi provisioning moved entirely to NVS via `tools/wifi_seed.py`.
- API key storage for service authentication.

## Phase 0 — OS / app isolation foundation

- Two-level launcher (category → app list → app) in portrait mode.
- `AppDescriptor` + `STICK_REGISTER_APP` registry.
- OS-owned status strip, `AppContext` content rect.
- Short PWR-click exits via `stick_os::checkAppExit()`.
- Initial game set: flappy, dino, scream, galaxy, balance, simon, panic.
- `scribe` audio-streaming app with WebSocket + Gemini transcription.
