#include "stick_ota.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include <stick_config.h>

#include "stick_store.h"  // reuse the API key if the server requires it

namespace stick_os {

// ---- minimal JSON field extractor ----
// We only need scalar string/int fields. Full JSON parsing would pull
// in ArduinoJson (tens of KB); for a 4-field response a linear scan is
// plenty. Expects "key":"value" or "key":123 patterns in the payload.

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

static bool jsonFindInt(const String& body, const char* key, uint32_t* out) {
    String pat = String("\"") + key + "\"";
    int k = body.indexOf(pat);
    if (k < 0) return false;
    int colon = body.indexOf(':', k);
    if (colon < 0) return false;
    // Skip whitespace
    int p = colon + 1;
    while (p < (int)body.length() && (body[p] == ' ' || body[p] == '\t')) p++;
    if (p >= (int)body.length()) return false;
    // Parse digits
    uint32_t v = 0;
    bool any = false;
    while (p < (int)body.length() && body[p] >= '0' && body[p] <= '9') {
        v = v * 10 + (body[p] - '0');
        p++;
        any = true;
    }
    if (!any) return false;
    *out = v;
    return true;
}

// ---- host helpers ----

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

// ---- public API ----

bool otaCheckForUpdate(OtaInfo* out) {
    if (out == nullptr) return false;
    memset(out, 0, sizeof(*out));

    NetworkClientSecure client;
    client.setInsecure();  // Phase 2h uses HTTPS against a self-signed/
                            // private host; pin the cert in a later pass.
    HTTPClient http;

    String url = buildHostUrl("/api/firmware");
    Serial.printf("[ota] GET %s\n", url.c_str());
    if (!http.begin(client, url)) return false;
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[ota] http %d\n", code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    if (!jsonFindString(body, "version", out->version, sizeof(out->version))) {
        Serial.println("[ota] missing version");
        return false;
    }
    jsonFindString(body, "path",   out->path,   sizeof(out->path));
    jsonFindString(body, "sha256", out->sha256, sizeof(out->sha256));
    jsonFindInt   (body, "size",   &out->size);

    Serial.printf("[ota] server: v=%s path=%s size=%u\n",
                  out->version, out->path, (unsigned)out->size);
    return true;
}

bool otaDownloadAndApply(const OtaInfo* info, OtaProgressFn progress) {
    if (info == nullptr || info->path[0] == 0 || info->size == 0) return false;

    // Hex-decode the expected SHA256 so we can compare bytes at the end.
    uint8_t expected[32];
    if (strlen(info->sha256) != 64) {
        Serial.println("[ota] missing/bad sha256");
        return false;
    }
    for (int i = 0; i < 32; i++) {
        char hi = info->sha256[i * 2], lo = info->sha256[i * 2 + 1];
        auto fromHex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            if (c >= 'A' && c <= 'F') return 10 + c - 'A';
            return -1;
        };
        int h = fromHex(hi), l = fromHex(lo);
        if (h < 0 || l < 0) {
            Serial.println("[ota] bad sha256 char");
            return false;
        }
        expected[i] = (uint8_t)((h << 4) | l);
    }

    NetworkClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = buildHostUrl(info->path);
    Serial.printf("[ota] downloading %s (%u bytes)\n", url.c_str(),
                  (unsigned)info->size);
    if (!http.begin(client, url)) return false;
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[ota] http %d\n", code);
        http.end();
        return false;
    }

    if (!Update.begin(info->size, U_FLASH)) {
        Serial.printf("[ota] Update.begin failed (err=%d)\n",
                      Update.getError());
        http.end();
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient* stream = http.getStreamPtr();
    const size_t CHUNK = 1024;
    uint8_t buf[CHUNK];
    uint32_t done = 0;
    uint32_t deadline = millis() + 120000;  // 2min global timeout
    while (done < info->size) {
        if (millis() > deadline) {
            Serial.println("[ota] timeout");
            break;
        }
        size_t avail = stream->available();
        if (avail == 0) {
            delay(5);
            continue;
        }
        size_t want = avail > CHUNK ? CHUNK : avail;
        size_t remaining = info->size - done;
        if (want > remaining) want = remaining;
        int n = stream->readBytes(buf, want);
        if (n <= 0) continue;
        if (Update.write(buf, n) != (size_t)n) {
            Serial.printf("[ota] write failed at %u\n", (unsigned)done);
            break;
        }
        mbedtls_sha256_update(&sha, buf, n);
        done += n;
        if (progress) progress(done, info->size);
    }

    http.end();

    uint8_t actual[32];
    mbedtls_sha256_finish(&sha, actual);
    mbedtls_sha256_free(&sha);

    if (done != info->size) {
        Update.abort();
        Serial.printf("[ota] incomplete: got %u / %u\n",
                      (unsigned)done, (unsigned)info->size);
        return false;
    }
    if (memcmp(actual, expected, 32) != 0) {
        Update.abort();
        Serial.println("[ota] sha256 mismatch");
        return false;
    }
    if (!Update.end(true)) {
        Serial.printf("[ota] Update.end failed (err=%d)\n", Update.getError());
        return false;
    }

    Serial.println("[ota] success — reboot to activate");
    return true;
}

}  // namespace stick_os
