#pragma once

// Public API of the Arcade app module.
//
// Exactly three symbols are exposed. All other functions, variables, and
// types live in the namespace-internal scope of arcade_app.cpp and must
// not be referenced from outside.
namespace ArcadeApp {

// Called once after the sketch wrapper calls StickCP2.begin() + Serial.begin().
// Owns: display rotation, EEPROM, WiFi/NTP (via StickNet), audio setup.
// Must tolerate peripherals being in an arbitrary state on entry.
void init();

// Called every loop() iteration from the sketch wrapper.
// Must return promptly (< ~50ms). No infinite loops.
void tick();

// Draw a small identifying glyph at (x, y) using the given color.
void icon(int x, int y, uint16_t color);

}  // namespace ArcadeApp
