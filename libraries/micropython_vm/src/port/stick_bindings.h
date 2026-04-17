#pragma once

// Plain-C interface between the mod_stick Python module (C) and the
// Arduino/C++ runtime code that drives the actual hardware.
//
// Every symbol here is a forward-compatibility promise — the Python-side
// contract (stick.* api_version 1) is built around these names. Adding
// new functions is safe; renaming or removing is not.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ---- Timing ----
uint32_t stick_bind_millis(void);
void     stick_bind_delay(uint32_t ms);

// ---- Exit flag ----
bool     stick_bind_should_exit(void);

// ---- Display ----
// Coordinates are in the app's content rect (OS status strip is already
// excluded — apps never need to add the 18px offset). Colors are RGB565.
int      stick_bind_display_width(void);
int      stick_bind_display_height(void);
void     stick_bind_display_fill(uint16_t color);
void     stick_bind_display_rect(int x, int y, int w, int h, uint16_t color);
void     stick_bind_display_line(int x0, int y0, int x1, int y1, uint16_t color);
void     stick_bind_display_pixel(int x, int y, uint16_t color);
void     stick_bind_display_text(const char* s, int x, int y, uint16_t color);
void     stick_bind_display_text2(const char* s, int x, int y, uint16_t color);

// ---- Buttons ----
void     stick_bind_buttons_update(void);
bool     stick_bind_buttons_a_pressed(void);
bool     stick_bind_buttons_b_pressed(void);

// ---- IMU ----
void     stick_bind_imu_accel(float* x, float* y, float* z);
void     stick_bind_imu_gyro(float* x, float* y, float* z);

// ---- Store (per-app scoped NVS) ----
size_t   stick_bind_store_get(const char* key, char* out, size_t out_size,
                              const char* default_value);
bool     stick_bind_store_put(const char* key, const char* value);

#ifdef __cplusplus
}
#endif
