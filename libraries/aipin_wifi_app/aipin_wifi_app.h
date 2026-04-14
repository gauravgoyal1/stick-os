#pragma once

// Public API of the AiPin WiFi variant.
//
// Exactly three symbols are exposed. All other functions, variables, and
// types live in the namespace-internal scope of aipin_wifi_app.cpp and
// must not be referenced from outside.
namespace AiPinWifiApp {

// Called once after the sketch wrapper calls StickCP2.begin() + Serial.begin().
// Owns: display rotation, color init, WiFi scan+connect, mic/speaker setup,
// TCP client setup, runtime tuning command parser.
void init();

// Called every loop() iteration. Drains audio processing, handles input,
// services the TCP connection, and parses any pending serial tuning commands.
void tick();

}  // namespace AiPinWifiApp
