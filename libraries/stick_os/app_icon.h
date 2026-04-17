#pragma once

// Icon helpers shared by the launcher app list and the App Store.
// Every AppDescriptor has an optional icon function; when null, we draw
// a rounded tile with the first letter of the app name so every row still
// has a visual anchor.

#include <stdint.h>

namespace stick_os {

struct AppDescriptor;

// Draws a 32x32 icon slot at (x, y). Uses app->icon if non-null, else
// renders a letter-tile fallback in the same color.
void drawAppIconOrFallback(int x, int y, const AppDescriptor* app,
                            uint16_t color);

// Letter tile: rounded rect outline containing the uppercased first
// character of name (or '?' when name is empty).
void drawLetterTile(int x, int y, const char* name, uint16_t color);

}  // namespace stick_os
