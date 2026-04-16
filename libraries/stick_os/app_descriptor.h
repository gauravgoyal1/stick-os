#pragma once

#include <stdint.h>

namespace stick_os {

enum AppCategory : uint8_t {
    CAT_GAME     = 0,
    CAT_UTILITY  = 1,
    CAT_SENSOR   = 2,
    CAT_SETTINGS = 3,
    CAT_COUNT,
};

enum AppFlags : uint8_t {
    APP_NONE              = 0,
    APP_SYSTEM_LOCKED     = 1 << 0,  // cannot be uninstalled (reserved)
    APP_NEEDS_NET         = 1 << 1,
    APP_NEEDS_MIC         = 1 << 2,
    APP_NEEDS_IR          = 1 << 3,
};

// Allow natural combination: APP_NEEDS_NET | APP_NEEDS_MIC yields AppFlags.
inline constexpr AppFlags operator|(AppFlags a, AppFlags b) {
    return static_cast<AppFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline constexpr AppFlags operator&(AppFlags a, AppFlags b) {
    return static_cast<AppFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

enum RuntimeId : uint8_t {
    RUNTIME_NATIVE = 0,
    RUNTIME_MPY    = 1,  // Phase 2
};

using IconDrawFn = void (*)(int x, int y, uint16_t color);

struct NativeEntry {
    void (*init)();
    void (*tick)();
    void (*suspend)();   // optional, nullptr allowed in Phase 0
    void (*resume)();    // optional, nullptr allowed in Phase 0
};

struct ScriptEntry {
    const char* path;    // reserved for Phase 2
    const char* entry;
};

struct AppDescriptor {
    const char* id;          // stable slug: "arcade", "scribe", "flappy"
    const char* name;        // display name: "Arcade", "Scribe"
    const char* version;     // "1.0.0"
    AppCategory category;
    AppFlags    flags;
    IconDrawFn  icon;
    RuntimeId   runtime;
    NativeEntry native;      // Phase 0: only this is used
    ScriptEntry script;      // Phase 2
};

}  // namespace stick_os
