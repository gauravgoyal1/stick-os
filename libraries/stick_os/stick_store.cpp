#include "stick_store.h"

#include <string.h>

namespace stick_os {

StickStore::StickStore(const char* ns) : ns_(ns) {}

void StickStore::open_(bool readOnly) {
    prefs_.begin(ns_, readOnly);
}

void StickStore::close_() {
    prefs_.end();
}

uint8_t StickStore::getU8(const char* key, uint8_t defaultValue) {
    open_(true);
    uint8_t v = prefs_.getUChar(key, defaultValue);
    close_();
    return v;
}

uint32_t StickStore::getU32(const char* key, uint32_t defaultValue) {
    open_(true);
    uint32_t v = prefs_.getULong(key, defaultValue);
    close_();
    return v;
}

bool StickStore::getBool(const char* key, bool defaultValue) {
    open_(true);
    bool v = prefs_.getBool(key, defaultValue);
    close_();
    return v;
}

size_t StickStore::getStr(const char* key, char* out, size_t outSize, const char* defaultValue) {
    if (outSize == 0) return 0;
    open_(true);
    size_t n = prefs_.getString(key, out, outSize);
    close_();
    if (n == 0 && defaultValue != nullptr) {
        strncpy(out, defaultValue, outSize - 1);
        out[outSize - 1] = '\0';
        n = strlen(out);
    }
    return n;
}

void StickStore::putU8(const char* key, uint8_t value) {
    open_(false); prefs_.putUChar(key, value); close_();
}

void StickStore::putU32(const char* key, uint32_t value) {
    open_(false); prefs_.putULong(key, value); close_();
}

void StickStore::putBool(const char* key, bool value) {
    open_(false); prefs_.putBool(key, value); close_();
}

void StickStore::putStr(const char* key, const char* value) {
    open_(false); prefs_.putString(key, value); close_();
}

void StickStore::clear() {
    open_(false); prefs_.clear(); close_();
}

size_t loadWiFiCreds(WiFiCred* out, size_t maxCount) {
    StickStore s("wifi_creds");
    size_t count = s.getU8("count", 0);
    if (count > maxCount) count = maxCount;
    for (size_t i = 0; i < count; i++) {
        char keyS[8], keyP[8];
        snprintf(keyS, sizeof(keyS), "s%u", (unsigned)i);
        snprintf(keyP, sizeof(keyP), "p%u", (unsigned)i);
        s.getStr(keyS, out[i].ssid, sizeof(out[i].ssid));
        s.getStr(keyP, out[i].pass, sizeof(out[i].pass));
    }
    return count;
}

bool saveWiFiCred(const char* ssid, const char* pass) {
    StickStore s("wifi_creds");
    size_t count = s.getU8("count", 0);
    // Check if already exists — update in place
    for (size_t i = 0; i < count; i++) {
        char keyS[8], buf[33];
        snprintf(keyS, sizeof(keyS), "s%u", (unsigned)i);
        s.getStr(keyS, buf, sizeof(buf));
        if (strcmp(buf, ssid) == 0) {
            char keyP[8];
            snprintf(keyP, sizeof(keyP), "p%u", (unsigned)i);
            s.putStr(keyP, pass);
            return true;
        }
    }
    if (count >= kMaxWiFiNetworks) return false;
    char keyS[8], keyP[8];
    snprintf(keyS, sizeof(keyS), "s%u", (unsigned)count);
    snprintf(keyP, sizeof(keyP), "p%u", (unsigned)count);
    s.putStr(keyS, ssid);
    s.putStr(keyP, pass);
    s.putU8("count", count + 1);
    return true;
}

bool deleteWiFiCred(const char* ssid) {
    StickStore s("wifi_creds");
    size_t count = s.getU8("count", 0);
    for (size_t i = 0; i < count; i++) {
        char keyS[8], buf[33];
        snprintf(keyS, sizeof(keyS), "s%u", (unsigned)i);
        s.getStr(keyS, buf, sizeof(buf));
        if (strcmp(buf, ssid) == 0) {
            // Shift remaining entries down
            for (size_t j = i; j < count - 1; j++) {
                char kS1[8], kP1[8], kS2[8], kP2[8], tmpS[33], tmpP[65];
                snprintf(kS1, sizeof(kS1), "s%u", (unsigned)(j + 1));
                snprintf(kP1, sizeof(kP1), "p%u", (unsigned)(j + 1));
                snprintf(kS2, sizeof(kS2), "s%u", (unsigned)j);
                snprintf(kP2, sizeof(kP2), "p%u", (unsigned)j);
                s.getStr(kS1, tmpS, sizeof(tmpS));
                s.getStr(kP1, tmpP, sizeof(tmpP));
                s.putStr(kS2, tmpS);
                s.putStr(kP2, tmpP);
            }
            s.putU8("count", count - 1);
            return true;
        }
    }
    return false;
}

void setLastConnectedSSID(const char* ssid) {
    StickStore s("wifi_creds");
    s.putStr("last", ssid);
}

bool getLastConnectedSSID(char* out, size_t outSize) {
    StickStore s("wifi_creds");
    return s.getStr("last", out, outSize) > 0;
}

bool saveApiKey(const char* key) {
    StickStore s("stick_auth");
    s.putStr("apikey", key);
    return true;
}

bool getApiKey(char* out, size_t outSize) {
    StickStore s("stick_auth");
    return s.getStr("apikey", out, outSize) > 0;
}

}  // namespace stick_os
