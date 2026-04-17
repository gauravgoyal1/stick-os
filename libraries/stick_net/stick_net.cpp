#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <stick_store.h>
#include "stick_net.h"

namespace StickNet {

namespace {

volatile Stage  g_stage     = STAGE_IDLE;
volatile bool   g_ntpSynced = false;
TaskHandle_t    g_task      = nullptr;
SemaphoreHandle_t g_mutex   = nullptr;

// Try WiFi.begin + wait for WL_CONNECTED. Disconnects from any prior AP
// first so the radio is in a clean state before the new attempt.
static bool tryBeginAndWait(const char* ssid, const char* pass,
                             int attempts) {
    Serial.printf("[StickNet] trying: %s\n", ssid);
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (pass && *pass) WiFi.begin(ssid, pass);
    else               WiFi.begin(ssid);
    for (int i = 0; i < attempts; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[StickNet] connected to %s\n", ssid);
            return true;
        }
    }
    return false;
}

// Internal — assumes caller already holds g_mutex. Returns WL_CONNECTED.
//
// Skips the scan path entirely: after a failed WiFi.begin() the radio
// commonly reports scanNetworks()==0 even for networks that *are* in
// range (the picker only finds them after a second rescan). Blindly
// trying each saved credential avoids that failure mode and is fine
// for the handful of networks the stick typically has.
bool connectWiFiLocked() {
    WiFi.mode(WIFI_STA);

    stick_os::WiFiCred creds[stick_os::kMaxWiFiNetworks];
    size_t credCount = stick_os::loadWiFiCreds(creds, stick_os::kMaxWiFiNetworks);
    if (credCount == 0) {
        Serial.println("[StickNet] no stored credentials");
        return false;
    }

    char lastSSID[33] = {0};
    stick_os::getLastConnectedSSID(lastSSID, sizeof(lastSSID));

    // 1. Try the last-connected SSID first (fast path on every subsequent
    //    boot in a normal home-WiFi setup).
    if (lastSSID[0] != '\0') {
        for (size_t i = 0; i < credCount; i++) {
            if (strcmp(creds[i].ssid, lastSSID) == 0) {
                if (tryBeginAndWait(creds[i].ssid, creds[i].pass, 20)) {
                    return true;
                }
                break;
            }
        }
    }

    // 2. Retry every stored cred (including the one we just tried —
    //    the first attempt often fails with cold radio state, a second
    //    attempt after the disconnect+delay in tryBeginAndWait usually
    //    succeeds).
    for (size_t j = 0; j < credCount; j++) {
        if (tryBeginAndWait(creds[j].ssid, creds[j].pass, 20)) {
            stick_os::setLastConnectedSSID(creds[j].ssid);
            return true;
        }
    }

    Serial.println("[StickNet] all stored networks failed");
    return false;
}

bool syncNTPLocked() {
    if (g_ntpSynced) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    Serial.println("[StickNet] syncing NTP...");

    struct timeval tv = {0, 0};
    settimeofday(&tv, NULL);
    configTime(19800, 0, "time.google.com", "pool.ntp.org");
    vTaskDelay(pdMS_TO_TICKS(2000));

    struct tm timeinfo = {0};
    for (int attempts = 0; attempts < 10; attempts++) {
        if (getLocalTime(&timeinfo, 2000) && timeinfo.tm_year + 1900 >= 2024) {
            m5::rtc_datetime_t dt;
            dt.date.year    = timeinfo.tm_year + 1900;
            dt.date.month   = timeinfo.tm_mon + 1;
            dt.date.date    = timeinfo.tm_mday;
            dt.time.hours   = timeinfo.tm_hour;
            dt.time.minutes = timeinfo.tm_min;
            dt.time.seconds = timeinfo.tm_sec;
            StickCP2.Rtc.setDateTime(dt);
            Serial.printf("[StickNet] time synced %02d:%02d\n",
                          timeinfo.tm_hour, timeinfo.tm_min);
            g_ntpSynced = true;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.println("[StickNet] NTP sync failed");
    return false;
}

void bringUpTask(void*) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_stage = STAGE_WIFI;
    bool wifiOk = connectWiFiLocked();

    if (wifiOk) {
        g_stage = STAGE_NTP;
        syncNTPLocked();
        g_stage = STAGE_READY;
    } else {
        g_stage = STAGE_FAILED;
    }
    xSemaphoreGive(g_mutex);

    g_task = nullptr;
    vTaskDelete(nullptr);
}

void ensureMutex() {
    if (g_mutex == nullptr) g_mutex = xSemaphoreCreateMutex();
}

}  // namespace

void startAsync() {
    ensureMutex();
    if (g_stage != STAGE_IDLE) return;     // already kicked off
    g_stage = STAGE_WIFI;                  // transition out of IDLE atomically
    xTaskCreatePinnedToCore(
        bringUpTask, "stick_net", 4096, nullptr,
        /*priority=*/1, &g_task, /*core=*/0);
}

void waitForReady(uint32_t timeoutMs) {
    ensureMutex();
    // Poll g_stage rather than blocking on the mutex — startAsync() can
    // return before the bringUpTask has had a chance to take the mutex,
    // in which case a mutex-based wait wins the race and returns
    // immediately with WiFi still disconnected, spuriously opening the
    // picker.
    const uint32_t step = 100;
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        if (g_stage == STAGE_IDLE ||
            g_stage == STAGE_READY ||
            g_stage == STAGE_FAILED) return;
        vTaskDelay(pdMS_TO_TICKS(step));
        elapsed += step;
    }
}

Stage status()       { return g_stage; }
bool  isWiFiReady()  { return WiFi.status() == WL_CONNECTED; }
int   rssi()         { return isWiFiReady() ? WiFi.RSSI() : 0; }

bool connectWiFi() {
    ensureMutex();
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    bool ok = connectWiFiLocked();
    xSemaphoreGive(g_mutex);
    return ok;
}

bool syncNTP() {
    ensureMutex();
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    bool ok = syncNTPLocked();
    xSemaphoreGive(g_mutex);
    return ok;
}

bool isConnected() { return isWiFiReady(); }

const char* ssid() {
    static char buf[33] = {0};
    if (!isWiFiReady()) { buf[0] = '\0'; return buf; }
    String s = WiFi.SSID();
    strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

size_t scanNetworks(ScanResult* out, size_t maxResults) {
    ensureMutex();
    xSemaphoreTake(g_mutex, portMAX_DELAY);

    int n = WiFi.scanNetworks();
    size_t written = 0;
    for (int i = 0; i < n && written < maxResults; i++) {
        ScanResult& r = out[written++];
        String foundSsid = WiFi.SSID(i);
        strncpy(r.ssid, foundSsid.c_str(), sizeof(r.ssid) - 1);
        r.ssid[sizeof(r.ssid) - 1] = '\0';
        r.rssi    = static_cast<int8_t>(WiFi.RSSI(i));
        r.channel = static_cast<uint8_t>(WiFi.channel(i));
        stick_os::WiFiCred nvsCreds[stick_os::kMaxWiFiNetworks];
        size_t nvsCount = stick_os::loadWiFiCreds(nvsCreds, stick_os::kMaxWiFiNetworks);
        r.known = false;
        for (size_t j = 0; j < nvsCount; j++) {
            if (foundSsid == nvsCreds[j].ssid) { r.known = true; break; }
        }
    }
    WiFi.scanDelete();

    xSemaphoreGive(g_mutex);
    return written;
}

}  // namespace StickNet
