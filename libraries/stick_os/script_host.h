#pragma once

// ScriptHost bridges AppDescriptor with the MicroPython VM. Phase 2d.
//
// A scripted app has runtime = RUNTIME_MPY. When the launcher dispatches
// such an app, instead of calling native.init() it calls scriptRunFile()
// with the path to the entry .py file on LittleFS. ScriptHost loads the
// file, spins up the VM, executes the source, and tears down the VM
// before returning.
//
// The VM is re-created per app launch rather than persisted — this
// guarantees a clean heap and keeps memory pressure bounded even if
// scripts leak objects.

#include <stddef.h>

namespace stick_os {

// Run a Python source file from LittleFS. `heapBytes` sizes the VM GC
// heap (per-app budget). Returns true if the file was found and the VM
// launched; false for missing file, read error, or VM init failure.
// Script exceptions are printed to Serial but don't cause a false return.
bool scriptRunFile(const char* path, size_t heapBytes = 32 * 1024);

}  // namespace stick_os
