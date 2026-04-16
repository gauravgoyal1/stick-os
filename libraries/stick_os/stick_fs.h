#pragma once

#include <stddef.h>

namespace stick_os {

// Mounts the LittleFS partition used for scripted-app storage.
// Formats on first use. Call once from setup().
bool fsInit();

// True after a successful fsInit().
bool fsReady();

// Filesystem capacity in bytes (0 if not mounted).
size_t fsTotalBytes();

// Used bytes on the filesystem (0 if not mounted).
size_t fsUsedBytes();

}  // namespace stick_os
