#pragma once

// Plain-C interface between the mod_stick Python module (C) and the
// Arduino/C++ runtime code that drives the actual hardware. Lives in the
// MPY embed build path so mod_stick.c can #include it at QSTR-extraction
// time; the matching stick_bindings.cpp in libraries/micropython_vm/src/
// provides the C++ implementations.

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
// Returns true if the launcher has signalled the app should quit (PWR press).
bool     stick_bind_should_exit(void);

#ifdef __cplusplus
}
#endif
