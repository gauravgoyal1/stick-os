#pragma once

#include <stdint.h>

#include "stick_store.h"

namespace stick_os {

// Handed to each app via currentContext() during init()/tick(). Apps must
// never draw outside the (contentX, contentY, contentW, contentH) rect —
// the region outside it belongs to the OS status strip.
struct AppContext {
    int16_t  contentX;
    int16_t  contentY;
    int16_t  contentW;
    int16_t  contentH;
    StickStore* store;   // per-app scoped store; namespace = "app_<id>"
};

// Returns the context for the currently-executing app. Only meaningful
// while an app's init()/tick() is on the stack — outside that, the
// returned rect reflects whatever app was most recently active.
const AppContext& currentContext();

// OS-internal: installs a new context before calling an app entry point.
// Not for app code.
void _setCurrentContext(const AppContext& ctx);

// App exit mechanism. Games call checkAppExit() in their loops after
// calling StickCP2.update(). Returns true if the user pressed PWR —
// the game should immediately return from init(). stick.ino checks
// wasExitRequested() after init()/tick() returns.
bool checkAppExit();
bool wasExitRequested();
void clearExitRequest();

// Logs free heap to serial with a tag. Called by the OS on app
// enter/exit for memory-leak diagnostics.
void logHeap(const char* tag);

}  // namespace stick_os
