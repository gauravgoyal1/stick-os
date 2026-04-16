#include "app_registry.h"

#include <Arduino.h>
#include <string.h>

namespace stick_os {

namespace {
const AppDescriptor* g_apps[kMaxApps] = {nullptr};
size_t               g_appCount       = 0;
// How many entries were added at compile-time (via STICK_REGISTER_APP).
// Dynamic entries start at g_apps[g_staticCount]. Only dynamic entries
// can be removed by unregisterApp().
size_t               g_staticCount    = 0;
bool                 g_staticSealed   = false;
}  // namespace

AppRegistration::AppRegistration(const AppDescriptor* d) {
    if (d == nullptr) return;
    if (g_appCount >= kMaxApps) {
        Serial.printf("[stick_os] registry full, dropping app id=%s\n",
                      d->id ? d->id : "(null)");
        return;
    }
    g_apps[g_appCount++] = d;
    // Static registrations happen before main(). Once the first dynamic
    // call arrives (after setup()), we seal the static count.
    if (!g_staticSealed) g_staticCount = g_appCount;
}

size_t appCount() { return g_appCount; }

const AppDescriptor* appAt(size_t i) {
    return (i < g_appCount) ? g_apps[i] : nullptr;
}

size_t appCountInCategory(AppCategory category) {
    size_t n = 0;
    for (size_t i = 0; i < g_appCount; i++) {
        if (g_apps[i]->category == category) n++;
    }
    return n;
}

const AppDescriptor* appAtInCategory(AppCategory category, size_t i) {
    size_t seen = 0;
    for (size_t j = 0; j < g_appCount; j++) {
        if (g_apps[j]->category != category) continue;
        if (seen == i) return g_apps[j];
        seen++;
    }
    return nullptr;
}

const AppDescriptor* findAppById(const char* id) {
    if (id == nullptr) return nullptr;
    for (size_t i = 0; i < g_appCount; i++) {
        if (g_apps[i]->id && strcmp(g_apps[i]->id, id) == 0) return g_apps[i];
    }
    return nullptr;
}

// ---------- Phase 2: dynamic registration ----------

bool registerApp(const AppDescriptor* d) {
    g_staticSealed = true;  // no more static registrations after this
    if (d == nullptr || d->id == nullptr) return false;
    if (findAppById(d->id) != nullptr) {
        Serial.printf("[stick_os] duplicate id \"%s\", skipping\n", d->id);
        return false;
    }
    if (g_appCount >= kMaxApps) {
        Serial.printf("[stick_os] registry full, cannot add \"%s\"\n", d->id);
        return false;
    }
    g_apps[g_appCount++] = d;
    Serial.printf("[stick_os] registered dynamic app \"%s\"\n", d->id);
    return true;
}

bool unregisterApp(const char* id) {
    if (id == nullptr) return false;
    for (size_t i = g_staticCount; i < g_appCount; i++) {
        if (g_apps[i]->id && strcmp(g_apps[i]->id, id) == 0) {
            Serial.printf("[stick_os] unregistered \"%s\"\n", id);
            // Shift remaining dynamic entries down.
            for (size_t j = i; j + 1 < g_appCount; j++) {
                g_apps[j] = g_apps[j + 1];
            }
            g_apps[--g_appCount] = nullptr;
            return true;
        }
    }
    return false;
}

}  // namespace stick_os
