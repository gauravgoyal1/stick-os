# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AiPin is a Bluetooth device scanner and connection manager for the **M5StickC Plus 2** (ESP32-based microcontroller). It discovers both Classic Bluetooth and BLE devices, displays them in a scrollable list, and allows connecting to show device details.

## Build & Upload

This is an Arduino IDE sketch (single `.ino` file). No CLI build tooling is configured.

- **Board**: M5StickC Plus 2 (ESP32-PICO)
- **Required libraries**: M5StickCPlus2, BluetoothSerial (ESP32 core), ESP32 BLE Arduino (ESP32 core)
- **Compile & upload**: Use Arduino IDE with the M5StickCPlus2 board selected, or PlatformIO with the appropriate ESP32 board config
- **Display**: 240x135 pixels, used in landscape (rotation 1)
- **Inputs**: BtnA (front), BtnB (side)

## Architecture

Single-file state machine in `aipin.ino` with four states:

```
STATE_SCANNING → STATE_SCAN_RESULTS → STATE_CONNECTING → STATE_CONNECTED
                      ↑                                        |
                      └──────── disconnect / lost connection ───┘
```

**Scan flow**: Two-phase blocking scan — Classic BT discovery (5s via `BluetoothSerial::discover()`) then BLE scan (5s via `BLEScan::start()`). Results merged into a single `devices[]` array, deduplicated by MAC address, sorted named-first then by RSSI.

**Connection flow**: Classic BT connects via SPP (`SerialBT.connect(addr)`). BLE connects via GATT client (`BLEClient::connect()`). Connected screen shows different info per type — device class for Classic BT, service UUIDs for BLE.

**UI pattern**: All screens follow `drawHeader()` / content / `drawFooter()` structure. The header shows title + battery %. The footer shows BtnA/BtnB action hints. Device list supports scrolling with `scrollOffset` / `selectedIndex`.

## Key Constraints

- Max 20 scanned devices, max 10 BLE service UUIDs stored
- Classic BT and BLE stacks coexist via ESP32 dual-mode (`btStart()` enables `ESP_BT_MODE_BTDM`)
- `BluetoothSerial::begin("AiPin", true)` must be called before `BLEDevice::init()` — initialization order matters
- macOS devices only appear in Classic BT scan when their Bluetooth settings panel is open
