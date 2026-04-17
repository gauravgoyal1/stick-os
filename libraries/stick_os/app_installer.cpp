#include "app_installer.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include <stdlib.h>

#include "app_descriptor.h"
#include "app_registry.h"

namespace stick_os {

// ---- minimal JSON scalar extractor (same idea as stick_ota.cpp) ----
static bool jsonFindString(const String& body, const char* key,
                            char* out, size_t outSize) {
    String pat = String("\"") + key + "\"";
    int k = body.indexOf(pat);
    if (k < 0) return false;
    int colon = body.indexOf(':', k);
    if (colon < 0) return false;
    int q1 = body.indexOf('"', colon);
    if (q1 < 0) return false;
    int q2 = body.indexOf('"', q1 + 1);
    if (q2 < 0) return false;
    size_t n = (size_t)(q2 - q1 - 1);
    if (n + 1 > outSize) n = outSize - 1;
    memcpy(out, body.c_str() + q1 + 1, n);
    out[n] = '\0';
    return true;
}

// ---- installed descriptor bookkeeping ----
// We keep heap-allocated descriptors + all their owned strings in a
// linked list so uninstallApp() can free them. Limit: kMaxApps minus
// whatever's compiled in.

struct InstalledApp {
    AppDescriptor desc;
    char id_buf[32];
    char name_buf[24];
    char version_buf[16];
    char path_buf[96];
    InstalledApp* next;
};

static InstalledApp* g_head = nullptr;

static AppCategory categoryFromString(const char* s) {
    if (strcmp(s, "game") == 0)     return CAT_GAME;
    if (strcmp(s, "utility") == 0)  return CAT_UTILITY;
    // sensor/settings explicitly disallowed for MPY (see registerApp);
    // default to utility to keep the app usable instead of silently
    // dropping it.
    return CAT_UTILITY;
}

static bool readFile(const char* path, String& out) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    out = "";
    while (f.available()) out += (char)f.read();
    f.close();
    return true;
}

bool registerInstalledApp(const char* dirPath) {
    String manifestPath = String(dirPath) + "/manifest.json";
    String body;
    if (!readFile(manifestPath.c_str(), body)) return false;

    char id[32] = {0}, name[24] = {0}, version[16] = {0};
    char category[16] = {0}, entry[32] = {0};
    if (!jsonFindString(body, "id", id, sizeof(id))) return false;
    if (!jsonFindString(body, "name", name, sizeof(name))) {
        strncpy(name, id, sizeof(name) - 1);
    }
    if (!jsonFindString(body, "version", version, sizeof(version))) {
        strcpy(version, "0.0.0");
    }
    jsonFindString(body, "category", category, sizeof(category));
    if (!jsonFindString(body, "entry", entry, sizeof(entry))) {
        strcpy(entry, "main.py");
    }

    InstalledApp* app = (InstalledApp*)calloc(1, sizeof(InstalledApp));
    if (app == nullptr) return false;

    strncpy(app->id_buf, id, sizeof(app->id_buf) - 1);
    strncpy(app->name_buf, name, sizeof(app->name_buf) - 1);
    strncpy(app->version_buf, version, sizeof(app->version_buf) - 1);
    snprintf(app->path_buf, sizeof(app->path_buf), "%s/%s", dirPath, entry);

    app->desc = {
        /*id=*/       app->id_buf,
        /*name=*/     app->name_buf,
        /*version=*/  app->version_buf,
        /*category=*/ categoryFromString(category),
        /*flags=*/    APP_NONE,
        /*icon=*/     nullptr,
        /*runtime=*/  RUNTIME_MPY,
        /*native=*/   { nullptr, nullptr, nullptr, nullptr },
        /*script=*/   { app->path_buf, "main" },
    };

    if (!registerApp(&app->desc)) {
        free(app);
        return false;
    }

    app->next = g_head;
    g_head = app;
    return true;
}

static void removeDirRecursive(const char* path) {
    File dir = LittleFS.open(path, "r");
    if (!dir || !dir.isDirectory()) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        String childPath = String(path) + "/" + entry.name();
        bool isDir = entry.isDirectory();
        entry.close();
        if (isDir) {
            removeDirRecursive(childPath.c_str());
            LittleFS.rmdir(childPath);
        } else {
            LittleFS.remove(childPath);
        }
    }
    dir.close();
    LittleFS.rmdir(path);
}

size_t scanInstalledApps() {
    const char* root = "/apps";
    if (!LittleFS.exists(root)) return 0;
    File d = LittleFS.open(root, "r");
    if (!d || !d.isDirectory()) return 0;
    size_t n = 0;
    File entry;
    while ((entry = d.openNextFile())) {
        if (entry.isDirectory()) {
            String dirPath = String(root) + "/" + entry.name();
            entry.close();
            if (registerInstalledApp(dirPath.c_str())) {
                n++;
                Serial.printf("[installer] loaded %s\n", dirPath.c_str());
            }
        } else {
            entry.close();
        }
    }
    d.close();
    return n;
}

bool uninstallApp(const char* id) {
    if (id == nullptr) return false;
    InstalledApp** slot = &g_head;
    while (*slot) {
        if (strcmp((*slot)->id_buf, id) == 0) break;
        slot = &(*slot)->next;
    }
    if (*slot == nullptr) return false;

    if (!unregisterApp(id)) return false;

    InstalledApp* node = *slot;
    *slot = node->next;

    String dir = String("/apps/") + id;
    removeDirRecursive(dir.c_str());
    free(node);
    return true;
}

}  // namespace stick_os
