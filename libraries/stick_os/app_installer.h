#pragma once

// Scripted-app installer / scanner. Phase 2e.
//
// Convention on LittleFS: each installed app lives in its own directory
// under /apps/<id>/ with at least:
//   manifest.json   — metadata (id, name, version, category, entry, ...)
//   main.py         — the entry script (can also be main.mpy when the
//                     publish pipeline adds mpy-cross compilation)
//
// scanInstalledApps() walks /apps/ at boot, parses each manifest, and
// registers a dynamically-allocated AppDescriptor via registerApp(). The
// descriptor lives in heap memory owned by the installer — unregisterApp
// on its own doesn't free that memory, so use uninstallApp() to both
// remove from the registry and delete the files.

#include <stddef.h>

namespace stick_os {

// Count of apps registered from LittleFS this boot.
size_t scanInstalledApps();

// Remove an app by id: unregister, delete /apps/<id>/ recursively,
// free its heap-allocated descriptor. Returns true on success.
bool uninstallApp(const char* id);

}  // namespace stick_os
