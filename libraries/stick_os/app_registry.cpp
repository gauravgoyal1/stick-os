#include "app_registry.h"

#include <Arduino.h>
#include <string.h>

namespace stick_os {

namespace {
const AppDescriptor* g_apps[kMaxApps] = {nullptr};
size_t               g_appCount       = 0;
}  // namespace

AppRegistration::AppRegistration(const AppDescriptor* d) {
    if (d == nullptr) return;
    if (g_appCount >= kMaxApps) {
        // Silent truncation is worse than a loud boot log: the app is lost
        // but flash/RAM are intact, and the engineer sees the problem.
        Serial.printf("[stick_os] registry full, dropping app id=%s\n",
                      d->id ? d->id : "(null)");
        return;
    }
    g_apps[g_appCount++] = d;
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

}  // namespace stick_os
