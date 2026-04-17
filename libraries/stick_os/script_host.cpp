#include "script_host.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <micropython_vm.h>

#include "app_context.h"  // clearExitRequest

namespace stick_os {

bool scriptRunFile(const char* path, size_t heapBytes) {
    if (path == nullptr || *path == 0) return false;

    if (!LittleFS.exists(path)) {
        Serial.printf("[scripthost] not found: %s\n", path);
        return false;
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.printf("[scripthost] open failed: %s\n", path);
        return false;
    }

    size_t sz = f.size();
    // Sanity cap — scripts larger than 64 KB are almost certainly a bug;
    // fail loud rather than silently OOM.
    if (sz == 0 || sz > 64 * 1024) {
        f.close();
        Serial.printf("[scripthost] bad size %u for %s\n",
                      (unsigned)sz, path);
        return false;
    }

    char* buf = (char*)malloc(sz + 1);
    if (buf == nullptr) {
        f.close();
        Serial.printf("[scripthost] malloc %u bytes failed\n",
                      (unsigned)(sz + 1));
        return false;
    }
    size_t n = f.read((uint8_t*)buf, sz);
    f.close();
    buf[n] = '\0';

    Serial.printf("[scripthost] running %s (%u bytes, heap=%u)\n",
                  path, (unsigned)n, (unsigned)heapBytes);

    clearExitRequest();

    if (!MicroPythonVM::init(heapBytes)) {
        free(buf);
        Serial.println("[scripthost] VM init failed");
        return false;
    }
    MicroPythonVM::execStr(buf);
    MicroPythonVM::deinit();

    free(buf);
    Serial.println("[scripthost] script finished");
    return true;
}

}  // namespace stick_os
