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

extern "C" uint32_t stick_bind_millis(void) {
    return (uint32_t)millis();
}

extern "C" void stick_bind_delay(uint32_t ms) {
    delay(ms);
}

extern "C" bool stick_bind_should_exit(void) {
    // wasExitRequested() reads without clearing, so scripts can poll it
    // each frame. The launcher clears the flag on teardown.
    return stick_os::wasExitRequested();
}
