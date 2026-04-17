# Apps catalog

Every app that ships with stick-os, plus the reference scripted apps available via the Store. Each entry lists the controls, what (if anything) the app persists across power-cycles, and any hardware requirements the launcher enforces.

The top-level README has install/OTA flow and source layout; this file is purely a per-app reference.

## Conventions

- **A / B / PWR**: the three physical buttons on the M5StickC Plus 2. A is the large round button on the front; B is the smaller side button; PWR is the power button on the left edge.
- **PWR (short press)** always exits the current app back to the launcher. The app list below only highlights it when behavior inside an app is non-obvious.
- **Status strip**: the 18 px OS-owned strip at the top (WiFi icon, clock, battery). Apps can't draw over it.
- **Persisted**: stored in a per-app NVS namespace via `StickStore` — survives reboot, survives firmware OTA, not sent off-device. Cleared only by `Storage → clear` or an `esptool erase_flash`.
- **Hardware flags** (declared in the `AppDescriptor`): `APP_NEEDS_NET` gates the app on WiFi; `APP_NEEDS_MIC` and `APP_NEEDS_IR` signal intent (no gate yet, but tracked for future policy).

---

## Games

### Flappy

Side-scrolling pipe dodger. Tap **A** to flap; gravity does the rest. Hitting a pipe or the ground ends the round; A restarts.

- Category: Game
- Controls: **A** flap / restart · **PWR** exit
- Persisted: best score

### Dino

Chrome-style endless runner. **A** jumps over cacti, **B** ducks under birds. Speed ramps up over time.

- Category: Game
- Controls: **A** jump · **B** duck · **PWR** exit
- Persisted: best score

### Scream

Vocal arcade game — the louder you scream into the mic, the higher the avatar flies. Used to be a crowd-pleaser; still is.

- Category: Game · Requires: mic (`APP_NEEDS_MIC`)
- Controls: **voice** altitude · **A** restart · **PWR** exit
- Persisted: best score

### Galaxy

Top-down space shooter. Tilt the stick left/right to steer between asteroids and collect stars; auto-fire handles the lasers.

- Category: Game
- Controls: **IMU tilt** steer · **A** restart on game-over · **PWR** exit
- Persisted: best score

### Balance

Keep a ball centered in a target by levelling the stick. Drift out of the ring and it's over.

- Category: Game
- Controls: **IMU tilt** roll the ball · **PWR** exit
- Persisted: best (longest-held) score

### Simon

Classic color/sound memory game. Watch the sequence, repeat it with **A** and **B**. Adds one step per round.

- Category: Game
- Controls: **A** / **B** to echo the sequence · **PWR** exit
- Persisted: best streak

### Panic

Shake-to-score reflex game — the IMU measures how hard you shake and scales points accordingly. Short escalating rounds.

- Category: Game · Requires: mic (`APP_NEEDS_MIC` — also reacts to shouts)
- Controls: **shake** the stick · **PWR** exit
- Persisted: best score

---

## Apps (utilities)

### Scribe

Voice recorder + transcriber. Captures 8-bit / 8 kHz mono PCM, streams it over a secure WebSocket to the server, which writes a WAV and hands it to Gemini for speaker-diarized transcription. Transcripts are also browsable on-device.

- Category: Utility · Requires: mic, WiFi, API key (`APP_NEEDS_NET | APP_NEEDS_MIC`)
- Controls:
  - **A** start / stop recording
  - **B** open transcript history (list of past recordings)
  - In history list: **A** open transcript · **B** next · **PWR** exit
  - In transcript viewer: **A** scroll down · **B** scroll up · **PWR** exit
- Persisted: nothing on-device — recordings and transcripts live on the server under `storage/recordings/` and `storage/transcripts/`.
- Notes: 64 KB cap on in-memory transcript text; larger ones render with a "truncated" indicator. Audio pipeline: HPF → LPF → noise-gate → soft-clip → 16→8-bit downconversion.

### Timer

Stopwatch with centisecond resolution, lap counter, and start / stop / reset semantics.

- Category: Utility
- Controls: **A** start/stop · **B** reset · **PWR** exit
- Persisted: last elapsed time (so resumed session after app exit is possible)

### Light

Full-screen flashlight. Cycles through presets — white, warm, red, green, blue — so you can use it as a reading light, map light, or night-vision preserving red.

- Category: Utility · Flags: `APP_NEEDS_IR` (legacy tag; IR LED not used here)
- Controls: **A** cycle color · **PWR** exit
- Persisted: last-selected color

---

## Sensors

### Battery

Live battery voltage, computed percentage, and charging status. Useful when debugging power draw or confirming a charger is actually charging.

- Category: Sensor
- Controls: **PWR** exit
- Persisted: none

### IMU

Real-time accelerometer (g) and gyroscope (dps) readout from the onboard MPU6886. Three-axis values, refreshed each frame.

- Category: Sensor
- Controls: **PWR** exit
- Persisted: none

### Scan

Scans nearby WiFi networks and lists SSID, signal strength (dBm), encryption type, and channel. Paginated when more than ~8 networks are visible.

- Category: Sensor · Requires: WiFi (`APP_NEEDS_NET`)
- Controls: **A** rescan · **B** next page · **PWR** exit
- Persisted: none

### Mic

Real-time microphone level meter — shows peak, RMS, and an animated waveform. Good for setting gain expectations before a Scribe session.

- Category: Sensor · Requires: mic (`APP_NEEDS_MIC`)
- Controls: **PWR** exit
- Persisted: none

---

## Settings

### About

Device info: firmware version string, build date, MAC address, flash/RAM usage, chip revision.

- Category: Settings
- Controls: **PWR** exit

### WiFi

Shows the connected network, RSSI, and local IP. `A` cycles between known stored networks (triggers a reconnect).

- Category: Settings
- Controls: **A** switch network · **PWR** exit

### Storage

NVS, flash, heap, and LittleFS usage breakdown with colored segments — at a glance you can see how much room is left for scripted apps or NVS growth.

- Category: Settings
- Controls: **PWR** exit

### Apps

Paginated listing of every app currently registered in the runtime registry — both native (compiled-in) and scripted (loaded from LittleFS). Shows a colored dot for each app's category.

- Category: Settings
- Controls: **B** next page · **PWR** exit

### Time

Large clock readout with date. **A** forces an NTP resync against `time.google.com` / `pool.ntp.org`.

- Category: Settings · Requires: WiFi (for resync)
- Controls: **A** resync NTP · **PWR** exit

### Update

Firmware OTA. Checks `/api/firmware`, compares against the running version, and if newer, streams the binary into the inactive OTA slot, verifies SHA256, and reboots into the new slot. LittleFS survives — installed scripted apps persist across updates.

- Category: Settings · Requires: WiFi
- Controls: **A** check / install · **PWR** exit

### Store

On-device App Store for scripted apps. Fetches `/api/catalog`, renders a list with letter-tile icons, and shows a green checkmark on apps that are already installed.

- Category: Settings · Requires: WiFi
- Controls:
  - On a not-installed row: **A** install (HTTPS GET each file → SHA256 verify → write to LittleFS → register descriptor immediately)
  - On an installed row: **A** prompts `Uninstall?` — confirm with **A** (removes files + unregisters) or cancel with **B**
  - **B** next · **PWR** exit
- Persisted: installed apps live under `/apps/<id>/` on LittleFS.

---

## Downloadable scripted apps (MicroPython)

Source under `apps/`. Shipped in the server catalog; installed via Store or `./tools/push_app.py`. Constraints: scripted apps must be `game` or `utility`, 32 KB default GC heap per launch, API surface is `stick.*` as documented in the main README.

### snake

Classic grid snake. A random-walk food spawn keeps rounds varied; speed ramps with score.

- Category: Game
- Controls: **A** turn right · **B** turn left · **PWR** exit
- Persisted: best score (via `stick.store`)
- Notes: embed-build MicroPython has no `random` module — snake rolls its own LCG seeded from `stick.millis()`.

### tilt

IMU bubble level. A colored diamond slides inside a fixed frame as you tilt the stick; green when within ~1.7° of level, yellow to ~10°, red beyond. `A` zeroes the reference — useful when mounting on a surface that isn't a factory-flat reference.

- Category: Utility
- Controls: **A** zero reference · **PWR** exit
- Persisted: zero offsets (`zx`, `zy`)

---

## How apps register

Native apps live under `libraries/<app_id>/` and register at link time:

```cpp
static const stick_os::AppDescriptor kDesc = {
    "my_app", "My App", "1.0.0",
    stick_os::CAT_UTILITY, stick_os::APP_NEEDS_NET,
    &MyApp::icon, stick_os::RUNTIME_NATIVE,
    { &MyApp::init, &MyApp::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
```

Scripted apps live under `apps/<id>/` with a `manifest.json` + `main.py`; `stick_os::scanInstalledApps()` walks `/apps/` on LittleFS each boot and registers a dynamic `AppDescriptor` per directory. Adding a native app = new library dir + `#include` in `os/os.ino`. Adding a scripted app = directory under `apps/`, optional `./tools/publish_app.py` for server distribution.
