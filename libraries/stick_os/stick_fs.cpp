#include "stick_fs.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace stick_os {

static bool g_mounted = false;

bool fsInit() {
    if (g_mounted) return true;

    // Partition label "spiffs" matches partitions.csv. formatOnFail=true
    // auto-formats on first boot after a partition table change.
    if (!LittleFS.begin(/*formatOnFail=*/true, "/littlefs", 10, "spiffs")) {
        Serial.println("[stick_fs] LittleFS mount failed");
        return false;
    }

    g_mounted = true;
    Serial.printf("[stick_fs] mounted — %u KB used / %u KB total\n",
                  (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
    return true;
}

bool fsReady() { return g_mounted; }

size_t fsTotalBytes() {
    return g_mounted ? LittleFS.totalBytes() : 0;
}

size_t fsUsedBytes() {
    return g_mounted ? LittleFS.usedBytes() : 0;
}

}  // namespace stick_os
