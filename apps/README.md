# MicroPython apps for Stick OS

Source for scripted apps. Each app is a directory:

```
apps/<id>/
├── manifest.json   # id, name, version, entry, api_version, category, heap_budget
├── main.py         # entry point
└── icon.bin        # optional 32x32 RGB565 icon (not yet generated)
```

## Installing on a device

Two paths, depending on whether you're iterating or distributing:

- **Serial push (dev):** `./tools/push_app.py --port <port> --app apps/<id>` writes the directory to `/apps/<id>/` on LittleFS via the `FILE_PUT` protocol. Reboot (or remove + re-install) so `scanInstalledApps()` picks up the new manifest.
- **Catalog download (users):** drop the app into `server/storage/apps/<id>/`, add an entry to `server/storage/catalog.json`, and install from **Settings → Store** on the device.

Uninstall with `APP_RM <id>` over serial.

## `stick.*` API surface

Scripted apps target api_version 1:

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
