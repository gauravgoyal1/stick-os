// App Store — browses the server catalog, installs and uninstalls
// scripted apps. Phase 2g.
//
// Expected catalog.json shape (per-app entries extend the Phase 2 plan):
//   {
//     "version": 1,
//     "apps": [
//       {
//         "id": "snake", "name": "Snake", "version": "1.0.0",
//         "category": "game",
//         "description": "Classic snake",
//         "files": [
//           { "name": "manifest.json", "url": "/apps/snake/manifest.json",
//             "size": 256, "sha256": "..." },
//           { "name": "main.py", "url": "/apps/snake/main.py",
//             "size": 2000, "sha256": "..." }
//         ]
//       }
//     ]
//   }

#include <M5StickCPlus2.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>

#include <stick_config.h>
#include <stick_net.h>
#include <stick_os.h>

#include "app_store.h"

namespace AppStore {

struct CatalogEntry {
    char id[32];
    char name[24];
    char version[16];
    char description[80];
    // Files kept as a flat string of "name|url|size|sha256;" tuples to
    // avoid a second allocation per app. Parsed lazily on install.
    String files_blob;
};

static const size_t kMaxCatalog = 16;
static CatalogEntry g_catalog[kMaxCatalog];
static size_t g_catalogCount = 0;
static int g_cursor = 0;

static String g_status;
static bool g_statusError = false;

// ---- minimal JSON helpers (same shape as stick_ota / app_installer) ----

static bool scanString(const String& body, int from, const char* key,
                        char* out, size_t outSize, int* endPos) {
    String pat = String("\"") + key + "\"";
    int k = body.indexOf(pat, from);
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
    if (endPos) *endPos = q2 + 1;
    return true;
}

// ---- host / HTTP ----

static String buildHostUrl(const char* path) {
    const char* proto = (kStickServerPort == 443) ? "https://" : "http://";
    String url = String(proto) + kStickServerHost;
    if (kStickServerPort != 80 && kStickServerPort != 443) {
        url += ":"; url += String(kStickServerPort);
    }
    if (path[0] != '/') url += "/";
    url += path;
    return url;
}

static bool httpGetString(const String& url, String& body) {
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    if (!http.begin(tls, url)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    body = http.getString();
    http.end();
    return true;
}

// ---- catalog fetch ----

static bool fetchCatalog() {
    g_catalogCount = 0;
    String body;
    if (!httpGetString(buildHostUrl("/api/catalog"), body)) return false;

    // Split the "apps":[...] array by finding top-level object braces.
    int appsKey = body.indexOf("\"apps\"");
    if (appsKey < 0) return false;
    int bracket = body.indexOf('[', appsKey);
    if (bracket < 0) return false;

    int pos = bracket + 1;
    while (g_catalogCount < kMaxCatalog) {
        int objStart = body.indexOf('{', pos);
        if (objStart < 0) break;
        // Find matching close brace (shallow — catalog objects have one
        // nested "files" array, so track array depth + brace depth).
        int depth = 1, p = objStart + 1;
        while (p < (int)body.length() && depth > 0) {
            char c = body[p];
            if (c == '"') {
                // Skip over a quoted string.
                p++;
                while (p < (int)body.length() && body[p] != '"') {
                    if (body[p] == '\\' && p + 1 < (int)body.length()) p++;
                    p++;
                }
            } else if (c == '{') depth++;
            else if (c == '}') depth--;
            p++;
        }
        if (depth != 0) break;
        int objEnd = p;

        String obj = body.substring(objStart, objEnd);
        CatalogEntry& e = g_catalog[g_catalogCount];
        memset(&e, 0, sizeof(e));
        int tmp;
        if (!scanString(obj, 0, "id", e.id, sizeof(e.id), &tmp)) {
            pos = objEnd; continue;
        }
        if (!scanString(obj, 0, "name", e.name, sizeof(e.name), &tmp)) {
            strncpy(e.name, e.id, sizeof(e.name) - 1);
        }
        scanString(obj, 0, "version", e.version, sizeof(e.version), &tmp);
        scanString(obj, 0, "description", e.description, sizeof(e.description), &tmp);

        // Flatten the files array into our simple blob.
        int filesKey = obj.indexOf("\"files\"");
        e.files_blob = "";
        if (filesKey >= 0) {
            int fBracket = obj.indexOf('[', filesKey);
            if (fBracket >= 0) {
                int q = fBracket + 1;
                while (q < (int)obj.length()) {
                    int fStart = obj.indexOf('{', q);
                    if (fStart < 0) break;
                    int fEnd = obj.indexOf('}', fStart);
                    if (fEnd < 0) break;
                    String fObj = obj.substring(fStart, fEnd + 1);
                    char fName[32] = {0}, fUrl[96] = {0}, fSha[65] = {0};
                    int t;
                    scanString(fObj, 0, "name",   fName, sizeof(fName), &t);
                    scanString(fObj, 0, "url",    fUrl,  sizeof(fUrl),  &t);
                    scanString(fObj, 0, "sha256", fSha,  sizeof(fSha),  &t);
                    if (fName[0] && fUrl[0]) {
                        e.files_blob += fName;
                        e.files_blob += "|";
                        e.files_blob += fUrl;
                        e.files_blob += "|";
                        e.files_blob += fSha;
                        e.files_blob += ";";
                    }
                    q = fEnd + 1;
                }
            }
        }

        g_catalogCount++;
        pos = objEnd;
    }
    return true;
}

// ---- install ----

static bool hexEqual(const char* hex, const uint8_t* bytes) {
    if (hex[0] == 0) return true;  // catalog may omit sha256 during dev
    for (int i = 0; i < 32; i++) {
        char buf[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        unsigned v = strtoul(buf, nullptr, 16);
        if ((uint8_t)v != bytes[i]) return false;
    }
    return true;
}

static bool downloadFileTo(const String& url, const char* destPath,
                            const char* expectedSha256) {
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    if (!http.begin(tls, url)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    File f = LittleFS.open(destPath, "w");
    if (!f) { http.end(); return false; }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    int total = http.getSize();
    int got = 0;
    uint32_t deadline = millis() + 30000;
    while (millis() < deadline && (total <= 0 || got < total)) {
        size_t avail = stream->available();
        if (avail == 0) { delay(5); continue; }
        size_t want = avail > sizeof(buf) ? sizeof(buf) : avail;
        int n = stream->readBytes(buf, want);
        if (n <= 0) break;
        f.write(buf, n);
        mbedtls_sha256_update(&sha, buf, n);
        got += n;
    }
    f.close();
    http.end();

    uint8_t actual[32];
    mbedtls_sha256_finish(&sha, actual);
    mbedtls_sha256_free(&sha);

    if (!hexEqual(expectedSha256, actual)) {
        LittleFS.remove(destPath);
        return false;
    }
    return true;
}

static bool installEntry(const CatalogEntry& e) {
    // Ensure /apps/<id>/ exists.
    String dir = String("/apps/") + e.id;
    if (!LittleFS.exists("/apps")) LittleFS.mkdir("/apps");
    if (LittleFS.exists(dir)) {
        // Uninstall first so we don't keep stale files around.
        stick_os::uninstallApp(e.id);
    }
    LittleFS.mkdir(dir);

    // Walk the flattened files blob: name|url|sha256;name|url|sha256;...
    String blob = e.files_blob;
    while (blob.length() > 0) {
        int sep = blob.indexOf(';');
        String one = (sep < 0) ? blob : blob.substring(0, sep);
        blob = (sep < 0) ? String("") : blob.substring(sep + 1);

        int p1 = one.indexOf('|');
        int p2 = one.indexOf('|', p1 + 1);
        if (p1 < 0 || p2 < 0) continue;
        String name = one.substring(0, p1);
        String url  = one.substring(p1 + 1, p2);
        String sha  = one.substring(p2 + 1);
        String dest = dir + "/" + name;

        String fullUrl = url.startsWith("http") ? url : buildHostUrl(url.c_str());
        if (!downloadFileTo(fullUrl, dest.c_str(), sha.c_str())) {
            return false;
        }
    }

    return stick_os::registerInstalledApp(dir.c_str());
}

// ---- UI ----

static const uint16_t kAccent = 0x07E5;  // green-ish, Store brand

static bool isInstalled(const char* id) {
    const stick_os::AppDescriptor* d = stick_os::findAppById(id);
    return d != nullptr && d->runtime == stick_os::RUNTIME_MPY;
}

static void drawCheckmark(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Two-line ✓ inside a 12x12 box.
    d.drawLine(x + 1, y + 6, x + 4, y + 9, color);
    d.drawLine(x + 4, y + 9, x + 10, y + 2, color);
    d.drawLine(x + 1, y + 7, x + 4, y + 10, color);
    d.drawLine(x + 4, y + 10, x + 10, y + 3, color);
}

static void drawHeader() {
    auto& d = StickCP2.Display;
    d.fillScreen(BLACK);
    d.setTextSize(2);
    d.setTextColor(kAccent, BLACK);
    d.setCursor(8, 8);
    d.print("Store");
    d.drawFastHLine(0, 30, d.width(), d.color565(40, 40, 40));
}

static void drawList() {
    auto& d = StickCP2.Display;
    d.fillRect(0, 34, d.width(), d.height() - 34 - 28, BLACK);
    if (g_catalogCount == 0) {
        d.setTextSize(1);
        d.setTextColor(d.color565(120, 120, 120), BLACK);
        d.setCursor(10, 70);
        d.print("No apps in catalog");
        return;
    }

    // Match the launcher app-list layout: rowH 38, icon at x=10, name at
    // size-2 starting x=46, with a checkmark on the right for installed.
    const int rowH      = 38;
    const int firstY    = 36;
    const int maxRows   = 5;

    int start = g_cursor - 2;
    if (start < 0) start = 0;
    if (start > (int)g_catalogCount - maxRows)
        start = (int)g_catalogCount - maxRows;
    if (start < 0) start = 0;

    for (int i = 0; i < maxRows && (start + i) < (int)g_catalogCount; i++) {
        const CatalogEntry& e = g_catalog[start + i];
        const bool sel      = (start + i) == g_cursor;
        const bool installed = isInstalled(e.id);
        const int y         = firstY + i * rowH;
        const uint16_t fg   = sel ? kAccent : d.color565(100, 100, 100);
        const uint16_t bg   = sel ? d.color565(15, 25, 15) : BLACK;

        d.drawRoundRect(4, y, d.width() - 8, rowH - 4, 4, fg);
        if (sel) d.fillRoundRect(5, y + 1, d.width() - 10, rowH - 6, 4, bg);

        // Icon slot — the catalog doesn't ship custom icons, so we pass
        // a synthetic descriptor for the letter fallback.
        stick_os::AppDescriptor tmp = {};
        tmp.name = e.name;
        tmp.icon = nullptr;
        stick_os::drawAppIconOrFallback(10, y + 3, &tmp, fg);

        // Name, 7 chars max at size 2 (89 px from x=46).
        d.setTextSize(2);
        d.setTextColor(sel ? WHITE : fg, bg);
        d.setCursor(46, y + 10);
        char name[8];
        strncpy(name, e.name, 7); name[7] = '\0';
        d.print(name);

        if (installed) {
            drawCheckmark(d.width() - 18, y + 12, kAccent);
        }
    }

    // Scroll indicator
    if ((int)g_catalogCount > maxRows) {
        int trackH = maxRows * rowH;
        int thumbH = (trackH * maxRows) / (int)g_catalogCount;
        if (thumbH < 8) thumbH = 8;
        int thumbY = firstY +
            (trackH - thumbH) * g_cursor / (int)(g_catalogCount - 1);
        d.fillRect(d.width() - 3, firstY, 2, trackH,
                   d.color565(30, 30, 30));
        d.fillRect(d.width() - 3, thumbY, 2, thumbH,
                   d.color565(80, 80, 80));
    }
}

static void drawStatus() {
    auto& d = StickCP2.Display;
    d.fillRect(0, d.height() - 28, d.width(), 28, BLACK);
    d.setTextSize(1);
    if (g_status.length() > 0) {
        d.setTextColor(g_statusError ? RED : kAccent, BLACK);
        d.setCursor(6, d.height() - 24);
        d.print(g_status.c_str());
    }
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(6, d.height() - 12);
    const char* action = (g_catalogCount > 0 &&
                          isInstalled(g_catalog[g_cursor].id))
                         ? "A:uninst" : "A:install";
    d.printf("%s B:next PWR:back", action);
}

static void setStatus(const char* msg, bool error = false) {
    g_status = msg;
    g_statusError = error;
    drawStatus();
}

// Full-screen "Uninstall <name>?  A:yes  B:no" confirm. Blocks until the
// user chooses; returns true for yes. PWR also counts as no.
static bool confirmUninstall(const CatalogEntry& e) {
    auto& d = StickCP2.Display;
    d.fillScreen(BLACK);
    d.setTextSize(2);
    d.setTextColor(RED, BLACK);
    d.setCursor(8, 30);
    d.print("Uninstall?");
    d.setTextSize(1);
    d.setTextColor(WHITE, BLACK);
    d.setCursor(8, 64);
    d.print(e.name);
    d.setTextColor(d.color565(120, 120, 120), BLACK);
    d.setCursor(8, 84);
    d.print("Removes all files");
    d.setCursor(8, 98);
    d.print("and uninstalls app.");
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 30);
    d.print("A: yes");
    d.setCursor(8, d.height() - 16);
    d.print("B: no  PWR: back");

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return false;
        if (StickCP2.BtnA.wasPressed()) return true;
        if (StickCP2.BtnB.wasPressed()) return false;
        delay(20);
    }
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    g_cursor = 0;
    g_status = "";
    g_statusError = false;

    drawHeader();
    setStatus("Fetching catalog...");
    if (!StickNet::isConnected()) {
        setStatus("No WiFi", true);
    } else if (fetchCatalog()) {
        char line[32];
        snprintf(line, sizeof(line), "%u apps available",
                 (unsigned)g_catalogCount);
        setStatus(line);
    } else {
        setStatus("Catalog unreachable", true);
    }
    drawList();
    drawStatus();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnB.wasPressed() && g_catalogCount > 0) {
            g_cursor = (g_cursor + 1) % g_catalogCount;
            drawList();
            drawStatus();
        }
        if (StickCP2.BtnA.wasPressed() && g_catalogCount > 0) {
            const CatalogEntry& e = g_catalog[g_cursor];
            if (isInstalled(e.id)) {
                if (confirmUninstall(e)) {
                    drawHeader();
                    setStatus((String("Removing ") + e.name).c_str());
                    bool ok = stick_os::uninstallApp(e.id);
                    drawList();
                    setStatus(ok ? (String("Removed ") + e.name).c_str()
                                 : (String("Remove failed: ") + e.name).c_str(),
                              !ok);
                } else {
                    // Redraw after confirm screen.
                    drawHeader();
                    drawList();
                    drawStatus();
                }
            } else {
                setStatus((String("Installing ") + e.name).c_str());
                bool ok = installEntry(e);
                drawList();
                setStatus(ok ? (String("Installed ") + e.name).c_str()
                             : (String("Install failed: ") + e.name).c_str(),
                          !ok);
            }
        }
        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Shopping-bag glyph
    d.drawRoundRect(x + 4, y + 8, 22, 18, 2, color);
    d.drawArc(x + 15, y + 8, 7, 5, 180, 360, color);
}

}  // namespace AppStore

static const stick_os::AppDescriptor kDesc = {
    "store", "Store", "1.0.0",
    stick_os::CAT_SETTINGS,
    stick_os::APP_SYSTEM_LOCKED | stick_os::APP_NEEDS_NET,
    &AppStore::icon, stick_os::RUNTIME_NATIVE,
    { &AppStore::init, &AppStore::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
