#pragma once

#include <stdint.h>

// Shared WiFi + NTP helpers used by both arcade_app and aipin_wifi_app.
//
// No display I/O — callers own their own UI. Keep this surface minimal;
// add new symbols only when a second sketch needs them.
namespace StickNet {

// Stages the background bring-up task reports via status().
enum Stage : uint8_t {
    STAGE_IDLE     = 0,  // startAsync() never called
    STAGE_WIFI     = 1,  // scanning / associating
    STAGE_NTP      = 2,  // WiFi up, syncing time
    STAGE_READY    = 3,  // WiFi + NTP both done
    STAGE_FAILED   = 4,  // WiFi failed; NTP skipped
};

// Spawn a FreeRTOS task that runs connectWiFi() then syncNTP(). Idempotent:
// spawns the task at most once per boot. Returns immediately. Callers can
// render UI while the task runs in the background.
void startAsync();

// Blocks until the background bring-up task has finished (or timeout).
// Cheap if the task already completed or was never started. Callers that
// need WiFi before proceeding should call this before connectWiFi().
void waitForReady(uint32_t timeoutMs = 15000);

// Current bring-up stage — used by the launcher to render status.
Stage status();

// True iff WiFi is currently associated (WL_CONNECTED).
bool isWiFiReady();

// Current RSSI, or 0 if not connected. Used for status icons.
int rssi();

// Synchronous connect — scans, associates. Waits for any in-flight
// background task to finish first so it is safe to call from tick()
// reconnect paths. Returns true iff WL_CONNECTED on exit.
bool connectWiFi();

// Configure NTP (IST offset), wait for a plausible time, set the M5 RTC.
// Idempotent: only runs the network sync once per boot — later calls
// return the cached result. Returns true iff RTC was set.
bool syncNTP();

}  // namespace StickNet
