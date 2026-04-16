#pragma once

#include <stddef.h>

#include "app_descriptor.h"

namespace stick_os {

// Maximum number of apps that can register at compile time. Bump only
// when the current value is genuinely exceeded — the whole point of a
// fixed ceiling is to make growth a deliberate choice.
inline constexpr size_t kMaxApps = 32;

// Internal: registration RAII helper. Not meant to be instantiated by hand;
// use STICK_REGISTER_APP instead.
struct AppRegistration {
    explicit AppRegistration(const AppDescriptor* d);
};

// Returns the number of apps currently registered.
size_t appCount();

// Returns the i-th registered app (in registration order). nullptr if i
// is out of range. Stable for the lifetime of the process.
const AppDescriptor* appAt(size_t i);

// Returns the number of apps registered under `category`. O(n).
size_t appCountInCategory(AppCategory category);

// Returns the i-th app registered under `category` (in registration order).
// nullptr if i is out of range. O(n) — fine for the tiny n we have.
const AppDescriptor* appAtInCategory(AppCategory category, size_t i);

// Find an app by its `id` string. nullptr if not found.
const AppDescriptor* findAppById(const char* id);

// ---------- Phase 2: dynamic registration ----------

// Register an app at runtime (e.g. scripted apps discovered on LittleFS).
// The descriptor must remain valid for the lifetime of the registration —
// callers typically heap-allocate it. Returns false if the registry is full
// or the id is already registered.
bool registerApp(const AppDescriptor* d);

// Unregister a dynamically-registered app by id. Returns true if found and
// removed. Compile-time (STICK_REGISTER_APP) entries cannot be removed.
bool unregisterApp(const char* id);

}  // namespace stick_os

// Place this in any app library's .cpp to register the app at boot.
// Usage:
//     static const stick_os::AppDescriptor kDesc = { ... };
//     STICK_REGISTER_APP(kDesc);
#define STICK_REGISTER_APP(desc_identifier) \
    static ::stick_os::AppRegistration \
        _stick_app_reg_##desc_identifier(&(desc_identifier))
