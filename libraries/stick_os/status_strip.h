#pragma once

#include <stdint.h>

namespace stick_os {

// The OS-owned status strip occupies the top kStatusStripHeight pixels of
// the landscape display. Apps are forbidden from drawing into this region;
// they receive a content rect via AppContext that already excludes it.
inline constexpr int16_t kStatusStripHeight = 18;

// Initialize fonts/colors used by the strip. Idempotent.
void statusStripInit();

// Full repaint of the strip. Called once on entering the launcher or an
// app. Callers should not rely on this being cheap.
void statusStripDrawFull();

// Incremental update, cheap, safe to call every frame. Only redraws the
// WiFi bars + clock + battery if their displayed values actually changed.
// The `appName` pointer, if non-null, is rendered in the left portion of
// the strip; pass nullptr on the launcher's category picker / app list.
void statusStripTick(const char* appName);

}  // namespace stick_os
