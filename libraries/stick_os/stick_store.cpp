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

}  // namespace stick_os
