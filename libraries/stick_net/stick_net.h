#pragma once

#include <stddef.h>
#include <stdint.h>

// Shared WiFi + NTP helpers used by app libraries and the stick OS.
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

// Compatibility alias for callers that prefer the shorter name.
// Equivalent to isWiFiReady().
bool isConnected();

// Current SSID if connected, empty string otherwise.
// Returned pointer is valid until the next WiFi state change; callers should
// copy into their own buffer if they need to hold it across ticks.
const char* ssid();

// Small POD describing one scan result — keeps the app side independent of
// WiFi.h's result types.
struct ScanResult {
  char    ssid[33];   // max SSID length + null terminator
  int8_t  rssi;
  uint8_t channel;
  bool    known;      // true iff this SSID matches a wifi_config entry
};

// Blocking scan. Writes up to `maxResults` entries to `out`. Returns the
// number actually written (may be less than the number of networks visible
// if `maxResults` is small). Safe to call from app tick() paths — coordinates
// with the background bring-up task via the same mutex as connectWiFi().
size_t scanNetworks(ScanResult* out, size_t maxResults);

// Synchronous connect — scans, associates. Waits for any in-flight
// background task to finish first so it is safe to call from tick()
// reconnect paths. Returns true iff WL_CONNECTED on exit.
bool connectWiFi();

// Configure NTP (IST offset), wait for a plausible time, set the M5 RTC.
// Idempotent: only runs the network sync once per boot — later calls
// return the cached result. Returns true iff RTC was set.
bool syncNTP();

}  // namespace StickNet
