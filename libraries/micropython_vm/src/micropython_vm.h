#pragma once

// Thin C++ wrapper around MicroPython's embed port (mp_embed_*).
// Holds the heap allocation and a ready flag so callers don't have to
// reach into MicroPython internals.

#include <stddef.h>

namespace MicroPythonVM {

// Initialize the VM with the given heap size (bytes). Must be called
// before execStr(). Idempotent — returns true if already initialized.
bool init(size_t heapSize = 32 * 1024);

// Execute a Python source string. Errors are printed to Serial.
void execStr(const char* code);

// Tear down the VM and free the heap. Can be re-inited after.
void deinit();

// True between init() and deinit().
bool isReady();

}  // namespace MicroPythonVM
