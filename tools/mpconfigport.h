// Minimal MicroPython configuration for Stick OS embed.
// Copied to examples/embedding/mpconfigport.h by build_mpy_vm.sh
// before generating the embed port C sources.

#pragma once

// Common embed-port defaults (provides include paths, mp_hal_*, etc.)
#include <port/mpconfigport_common.h>

// Smallest ROM footprint as the starting point.
#define MICROPY_CONFIG_ROM_LEVEL                (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)

// Required — we feed raw Python source to the VM.
#define MICROPY_ENABLE_COMPILER                 (1)

// GC is required for any non-trivial script.
#define MICROPY_ENABLE_GC                       (1)
#define MICROPY_PY_GC                           (1)

// xtensa (ESP32) has no direct asm impl for gc_helper_get_regs —
// use the setjmp-based fallback.
#define MICROPY_GCREGS_SETJMP                   (1)

// xtensa `j` (jump) has limited range; the embed build can't place
// nlr_push_tail within reach. Use setjmp-based NLR for portability.
#define MICROPY_NLR_SETJMP                      (1)

// Skip embed_util.c's minimal __assert_func — newlib provides one.
#ifndef NDEBUG
#define NDEBUG
#endif

// Heap budget for scripted apps. Per-app sizing happens in Phase 2e.
#define MICROPY_HEAP_SIZE                       (32 * 1024)

// Float support — games and sensor readouts want it.
#define MICROPY_FLOAT_IMPL                      (MICROPY_FLOAT_IMPL_FLOAT)

// Keep strings ASCII-only to save flash.
#define MICROPY_PY_BUILTINS_STR_UNICODE         (0)

// Disable modules we don't ship on-device.
#define MICROPY_PY_IO                           (0)
#define MICROPY_PY_SYS                          (0)
#define MICROPY_PY_OS                           (0)

// The `stick` module gets registered in Task 2c; flag off during the spike.
// #define MICROPY_PY_STICK                     (1)
