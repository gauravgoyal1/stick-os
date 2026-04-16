#pragma once

#include <Preferences.h>
#include <stddef.h>
#include <stdint.h>

namespace stick_os {

// Thin wrapper around Preferences that scopes reads/writes to a per-app
// namespace. Apps never open Preferences directly — they use the
// StickStore handed to them via AppContext.
//
// The OS itself uses a StickStore bound to namespace "stick".
class StickStore {
public:
    explicit StickStore(const char* ns);

    uint8_t  getU8 (const char* key, uint8_t  defaultValue = 0);
    uint32_t getU32(const char* key, uint32_t defaultValue = 0);
    bool     getBool(const char* key, bool    defaultValue = false);
    size_t   getStr(const char* key, char* out, size_t outSize, const char* defaultValue = "");

    void putU8 (const char* key, uint8_t  value);
    void putU32(const char* key, uint32_t value);
    void putBool(const char* key, bool    value);
    void putStr(const char* key, const char* value);

    void clear();

private:
    const char* ns_;

    // Opens the namespace read-only or read-write. Each get/put call opens
    // and immediately closes — cheaper than keeping a handle open and
    // avoids cross-library contention for the single NVS handle.
    void open_(bool readOnly);
    void close_();
    Preferences prefs_;
};

// WiFi credential storage in NVS. Max 8 networks.
constexpr size_t kMaxWiFiNetworks = 8;

struct WiFiCred {
    char ssid[33];
    char pass[65];
};

size_t loadWiFiCreds(WiFiCred* out, size_t maxCount);
bool   saveWiFiCred(const char* ssid, const char* pass);
bool   deleteWiFiCred(const char* ssid);
void   setLastConnectedSSID(const char* ssid);
bool   getLastConnectedSSID(char* out, size_t outSize);

}  // namespace stick_os
