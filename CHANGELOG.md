# Changelog

Notable changes to stick-os. Phases follow the architecture roadmap: 0 = OS/app isolation foundation, 1 = sensors/settings/tools, 2 = scripted apps + OTA + App Store.

Each entry is annotated with the short hash of its primary commit (`git show <hash>` for the diff).

## Phase 2 — scripted apps, OTA, App Store (current)

**2j — Transcript history on device** (`cbdd1a4`, fixes `626cf34`). `server/api/transcripts.py` exposes `GET /api/transcripts` (`[{name, timestamp, size}, ...]`, newest-first) and `GET /api/transcripts/{name}` (filename-shape guarded, traversal-safe). Scribe's idle connected screen gains a history mode: **B** opens a list of past recordings; **A** fetches and renders the transcript word-wrapped; **A** scrolls down, **B** scrolls up. Body fetch is bounded at 64 KB to stay inside the ~200 KB free heap. HTTP path fixes: WebSocket is closed before the HTTPS GET so mbedTLS has a context slot free, the read loop breaks on `Content-Length` instead of waiting the full deadline (HTTP/1.1 keep-alive left connections open), and body append switched from per-byte to chunked `String::concat`.

**2i — Unified app-list UI + Store uninstall** (`ff98ecc`). New `libraries/stick_os/app_icon.h/.cpp` provides `drawAppIconOrFallback()`: uses the app's custom icon when present, otherwise a rounded letter tile with the first character of the name. Both the launcher app list and the App Store use it, so every row has a visual anchor. Store rows now mirror the launcher row style (rounded frame, size-2 name, letter tile) and render a green checkmark next to installed apps. Pressing **A** on an installed row opens an `Uninstall?` confirm screen — **A** to confirm (removes files via `stick_os::uninstallApp`), **B** to cancel.

**2h — Firmware OTA** (`f499493`). `libraries/stick_os/stick_ota.h/.cpp` fetches `/api/firmware`, streams the binary into the inactive OTA slot, verifies SHA256, and marks the slot as boot target. `libraries/settings_update/` adds the `Settings → Update` UI. LittleFS is untouched so installed scripted apps survive.

**2g — App Store** (`4735ab7`). `libraries/app_store/` adds a `Settings → Store` native app that parses `/api/catalog`, downloads each file with streaming SHA256 verification, writes to LittleFS, and calls `registerInstalledApp()`. New app installs without a reboot.

**2e — .stickapp installer** (`e62dae1`). `libraries/stick_os/app_installer.h/.cpp` walks `/apps/*/manifest.json` on boot and registers every scripted app into the runtime registry. `uninstallApp(id)` tears down the descriptor and recursively deletes the directory. `APP_RM <id>` serial command.

**2d — ScriptHost + launcher wiring** (`c5bbbf6`). `libraries/stick_os/script_host.h/.cpp` reads a `.py` file from LittleFS, inits the MPY VM, executes the source, tears down. `enterApp()` dispatches `RUNTIME_MPY` descriptors through ScriptHost. New dev serial commands: `FILE_PUT`, `FILE_LS`, `FILE_RM`, `MPY_RUN`. Scripted apps are restricted to `CAT_GAME` or `CAT_UTILITY` at the registry level.

**2c — `stick.*` Python bindings** (`4e565d1`, initial scaffold `8186e74`). `tools/user_c_modules/stick/` holds the canonical `mod_stick.c` + C++ bindings. Full api_version 1 surface: `display.fill/rect/line/pixel/text/text2/width/height`, `buttons.update/a_pressed/b_pressed`, `imu.accel/gyro`, `store.get/put`, `millis/delay/exit`, and the 7 RGB565 color constants.

**2b — MicroPython VM embed** (`2a9aa31`). `libraries/micropython_vm/` is generated from MicroPython v1.28.0 by `tools/build_mpy_vm.sh`. Measurement gate passed: +126 KB flash, 204 KB free heap after init. xtensa uses setjmp-based NLR and GC-regs fallbacks.

**2a — Partition table + LittleFS + dynamic registry** (`72fbce4`). `os/partitions.csv` (dual 3 MB OTA + 1.9 MB LittleFS). `stick_os::fsInit()` mounts LittleFS on boot. `registerApp()` and `unregisterApp()` extend the static `STICK_REGISTER_APP` pipeline with runtime registration.

### Tooling

- `tools/build_mpy_vm.sh` — clones MicroPython, regenerates embed sources with QSTR extraction for the `stick` module. (`2a9aa31`)
- `tools/push_file.py` / `push_app.py` — push files or whole app directories over USB serial. (shipped with `c5bbbf6`)
- `tools/publish_app.py` — copies `apps/<id>/` into `server/storage/apps/<id>/` and regenerates `catalog.json` with real sizes + sha256s. Run on the server host; restart uvicorn after a first-time publish into an empty storage tree so StaticFiles mounts `/apps`. (`f31c2d0`)
- `tools/gen_clangd.sh` — dumps the real `-I`/`-D` flags from `arduino-cli compile --verbose` into a `.clangd` at repo root so the editor LSP resolves M5Stack and ESP32 headers instead of producing "file not found" noise. `.clangd` is gitignored. (`6b3cac2`)
- Server test suite: 17 tests covering all HTTP endpoints + the Scribe WebSocket protocol. (`79aa222`)

### Rename

- `aipin_wifi_app` → `scribe` (client lib + server endpoint + recording file prefixes). (`b621144`, `19450c1`)
- `WIFI_SET` serial protocol switched from space to tab delimiter so SSIDs with spaces work. (`f719e40`)

### Reference scripted apps (`b19b1bb`)

- `apps/snake/` — classic grid snake, A=right, B=left, best-score in `stick.store`. Uses a millis-seeded LCG because the embed MicroPython build strips `random`. (rewrite in `ff98ecc`)
- `apps/tilt/` — IMU bubble level with a frame that redraws each bubble-move so the trailing erase doesn't chip the crosshairs. Formats its degree readout without `str.format` (not compiled into the embed build). (rewrite in `ff98ecc`)
- `apps/demo/` — dev smoke-test.

### Production deployment (`f137fd7`)

- `server/config/nginx.conf` — reverse-proxy template with WebSocket upgrade headers wired through for `/services/*`, HTTPS via Let's Encrypt, and a plain-HTTP → HTTPS redirect on :80. Drop in with `$STICK_DOMAIN` substituted.
- `server/config/stick-os-server.service` — systemd user-service unit that runs `uvicorn main:app` under the repo's virtualenv.
- `server/config/README.md` — setup walkthrough (issue certs, enable the service, tail logs).

### Reliability fixes

- **Scripted-app buttons** (`ff98ecc`). `stick.buttons.update()` and `stick.exit()` both called `StickCP2.update()` per frame, and the second call was clearing `wasPressed()` before the script could read it — A/B were silently dropped. Bindings now latch A/B presses in sticky flags that the `*_a_pressed()` / `*_b_pressed()` readers consume.
- **WiFi auto-connect** (`626cf34`). Scrapped the scan-then-match path (scan returned 0 too often after a failed `WiFi.begin`) in favour of trying each stored credential directly. `waitForReady` was racing the `bringUpTask`: startAsync could return before the task had a chance to take the mutex, so `waitForReady` grabbed it instantly and `setup()` opened the picker even as the task was still connecting. Now polls `g_stage` instead.
- **Scribe audio quality** (`627aa72`). 8-bit PCM WAV stores samples unsigned (centered at 128); firmware streams signed int8. The original `aipin/receiver.py` did `(b + 128) & 0xFF` before `writeframes`; the FastAPI rewrite dropped that step, flipping every recording's zero-crossing. Restored in `server/services/scribe.py`.

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
