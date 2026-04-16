# MicroPython apps for Stick OS

Source for Phase 2 scripted apps. Each app is a directory:

```
apps/<id>/
├── manifest.json   # id, name, version, entry, api_version, category, heap_budget
├── main.py         # entry point (compiled to main.mpy at publish time)
└── icon.bin        # optional 32x32 RGB565 icon (not yet generated)
```

At publish time (`tools/publish_app.sh`, not yet built), each directory is:
1. `mpy-cross main.py` → `main.mpy`
2. Tar of `manifest.json + main.mpy + icon.bin`
3. SHA256 + entry added to `stick-catalog/catalog.json`

On the device, the App Store (future) downloads the tar, extracts into
`/apps/<id>/` on LittleFS, and registers the descriptor via
`stick_os::registerApp()`.

## Runtime status

**Not runnable yet.** The MicroPython VM (`libraries/micropython_vm/`) is
the next Phase 2 gate — until it lands, these `.py` files are reference
source for the binding surface. The launcher currently shows "Scripted
apps not yet supported" for any descriptor with `runtime != RUNTIME_NATIVE`.

## `stick.*` API surface

These apps target `api_version=1` as defined in
[docs/superpowers/plans/2026-04-16-stick-os-phase2.md](../docs/superpowers/plans/2026-04-16-stick-os-phase2.md)
(Task 2c):

```python
stick.display.fill(color)
stick.display.rect(x, y, w, h, color)
stick.display.line(x0, y0, x1, y1, color)
stick.display.pixel(x, y, color)
stick.display.text(s, x, y, color)       # size 1
stick.display.text2(s, x, y, color)      # size 2
stick.display.width() / height()

stick.buttons.update()                   # call each frame
stick.buttons.a_pressed() / b_pressed()

stick.imu.accel() -> (x, y, z)           # g
stick.imu.gyro()  -> (x, y, z)           # dps

stick.store.get(key, default) / put(key, value)

stick.millis() / stick.delay(ms)
stick.exit() -> True when PWR pressed

# Colors
stick.BLACK, stick.WHITE, stick.RED, stick.GREEN,
stick.BLUE, stick.YELLOW, stick.CYAN
```
