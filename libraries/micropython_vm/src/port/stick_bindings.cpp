// C++ side of the stick.* module — concrete implementations backed by
// StickCP2 / stick_os. mod_stick.c calls these through the extern "C"
// declarations in stick_bindings.h.
//
// Installed into libraries/micropython_vm/src/port/ by build_mpy_vm.sh
// so it's part of the Arduino library compilation alongside mod_stick.c.

#include <Arduino.h>
#include <M5StickCPlus2.h>
#include <stick_os.h>

extern "C" {
#include "stick_bindings.h"
}

// ---- Timing ----

extern "C" uint32_t stick_bind_millis(void) {
    return (uint32_t)millis();
}

extern "C" void stick_bind_delay(uint32_t ms) {
    // Cooperative yield during long delays so StickCP2.update() gets
    // a chance to service the PWR-exit check.
    const uint32_t step = 10;
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms) {
        uint32_t remaining = ms - (millis() - start);
        delay(remaining < step ? remaining : step);
        if (stick_os::checkAppExit()) return;
    }
}

extern "C" bool stick_bind_should_exit(void) {
    // Poll hardware and latch the exit flag if PWR was pressed.
    stick_os::checkAppExit();
    return stick_os::wasExitRequested();
}

// ---- Display ----
// Apps see a content rect: the OS status strip (top 18px in portrait) is
// excluded. We translate coordinates here so scripts can use (0,0) as the
// top-left of their drawable area.

static inline int contentOffsetY() {
    // Portrait (rotation 0): strip is at the top.
    // Landscape apps would need a different translation; for Phase 2 all
    // scripted apps run in portrait, matching the launcher default.
    return stick_os::kStatusStripHeight;
}

extern "C" int stick_bind_display_width(void) {
    return StickCP2.Display.width();
}

extern "C" int stick_bind_display_height(void) {
    return StickCP2.Display.height() - contentOffsetY();
}

extern "C" void stick_bind_display_fill(uint16_t color) {
    auto& d = StickCP2.Display;
    d.fillRect(0, contentOffsetY(), d.width(),
               d.height() - contentOffsetY(), color);
}

extern "C" void stick_bind_display_rect(int x, int y, int w, int h,
                                         uint16_t color) {
    StickCP2.Display.fillRect(x, y + contentOffsetY(), w, h, color);
}

extern "C" void stick_bind_display_line(int x0, int y0, int x1, int y1,
                                         uint16_t color) {
    const int oy = contentOffsetY();
    StickCP2.Display.drawLine(x0, y0 + oy, x1, y1 + oy, color);
}

extern "C" void stick_bind_display_pixel(int x, int y, uint16_t color) {
    StickCP2.Display.drawPixel(x, y + contentOffsetY(), color);
}

extern "C" void stick_bind_display_text(const char* s, int x, int y,
                                         uint16_t color) {
    auto& d = StickCP2.Display;
    d.setTextSize(1);
    d.setTextColor(color);
    d.setCursor(x, y + contentOffsetY());
    d.print(s ? s : "");
}

extern "C" void stick_bind_display_text2(const char* s, int x, int y,
                                          uint16_t color) {
    auto& d = StickCP2.Display;
    d.setTextSize(2);
    d.setTextColor(color);
    d.setCursor(x, y + contentOffsetY());
    d.print(s ? s : "");
}

// ---- Buttons ----

extern "C" void stick_bind_buttons_update(void) {
    stick_os::checkAppExit();   // also polls StickCP2.update() via M5.update()
}

extern "C" bool stick_bind_buttons_a_pressed(void) {
    return StickCP2.BtnA.wasPressed();
}

extern "C" bool stick_bind_buttons_b_pressed(void) {
    return StickCP2.BtnB.wasPressed();
}

// ---- IMU ----

extern "C" void stick_bind_imu_accel(float* x, float* y, float* z) {
    StickCP2.Imu.update();
    auto d = StickCP2.Imu.getImuData();
    if (x) *x = d.accel.x;
    if (y) *y = d.accel.y;
    if (z) *z = d.accel.z;
}

extern "C" void stick_bind_imu_gyro(float* x, float* y, float* z) {
    StickCP2.Imu.update();
    auto d = StickCP2.Imu.getImuData();
    if (x) *x = d.gyro.x;
    if (y) *y = d.gyro.y;
    if (z) *z = d.gyro.z;
}

// ---- Store ----
// A fresh StickStore is opened for each call. Cheap (NVS handles are
// reused under the hood) and avoids global state. The namespace is
// "app_<id>"; since this spike runs before ScriptHost wires the current
// app id through, use a generic "mpy" namespace for now. Task 2d will
// switch this to the per-app namespace.

static stick_os::StickStore& scriptStore() {
    static stick_os::StickStore store("mpy");
    return store;
}

extern "C" size_t stick_bind_store_get(const char* key, char* out,
                                        size_t out_size,
                                        const char* default_value) {
    if (key == nullptr || out == nullptr || out_size == 0) return 0;
    return scriptStore().getStr(key, out, out_size,
                                default_value ? default_value : "");
}

extern "C" bool stick_bind_store_put(const char* key, const char* value) {
    if (key == nullptr || value == nullptr) return false;
    scriptStore().putStr(key, value);
    return true;
}
