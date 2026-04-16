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

// Internal — assumes caller already holds g_mutex. Returns WL_CONNECTED.
bool connectWiFiLocked() {
    WiFi.mode(WIFI_STA);

    // 1. Try last connected network first
    char lastSSID[33] = {0};
    if (stick_os::getLastConnectedSSID(lastSSID, sizeof(lastSSID)) && lastSSID[0] != '\0') {
        // Find password for this SSID in NVS
        stick_os::WiFiCred creds[stick_os::kMaxWiFiNetworks];
        size_t n = stick_os::loadWiFiCreds(creds, stick_os::kMaxWiFiNetworks);
        for (size_t i = 0; i < n; i++) {
            if (strcmp(creds[i].ssid, lastSSID) == 0) {
                Serial.printf("[StickNet] trying last: %s\n", lastSSID);
                WiFi.begin(lastSSID, creds[i].pass);
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 15) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    attempts++;
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("[StickNet] connected to %s\n", lastSSID);
                    return true;
                }
                break;
            }
        }
    }

    // 2. Scan and try known NVS networks
    int scanCount = WiFi.scanNetworks();
    if (scanCount <= 0) {
        Serial.println("[StickNet] no networks visible");
        return false;
    }

    stick_os::WiFiCred creds[stick_os::kMaxWiFiNetworks];
    size_t credCount = stick_os::loadWiFiCreds(creds, stick_os::kMaxWiFiNetworks);

    for (int i = 0; i < scanCount; i++) {
        String found = WiFi.SSID(i);
        for (size_t j = 0; j < credCount; j++) {
            if (found != creds[j].ssid) continue;
            Serial.printf("[StickNet] trying known: %s\n", creds[j].ssid);
            WiFi.begin(creds[j].ssid, creds[j].pass);
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 15) {
                vTaskDelay(pdMS_TO_TICKS(500));
                attempts++;
            }
            if (WiFi.status() == WL_CONNECTED) {
                stick_os::setLastConnectedSSID(creds[j].ssid);
                Serial.printf("[StickNet] connected to %s\n", creds[j].ssid);
                WiFi.scanDelete();
                return true;
            }
        }
    }

    // 3. Try any open network
    for (int i = 0; i < scanCount; i++) {
        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
            String openSSID = WiFi.SSID(i);
            Serial.printf("[StickNet] trying open: %s\n", openSSID.c_str());
            WiFi.begin(openSSID.c_str());
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) {
                vTaskDelay(pdMS_TO_TICKS(500));
                attempts++;
            }
            if (WiFi.status() == WL_CONNECTED) {
                stick_os::setLastConnectedSSID(openSSID.c_str());
                Serial.printf("[StickNet] connected to open: %s\n", openSSID.c_str());
                WiFi.scanDelete();
                return true;
            }
        }
    }

    WiFi.scanDelete();
    Serial.println("[StickNet] all networks failed");
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
    // Fast path: nothing to wait on.
    if (g_stage == STAGE_IDLE || g_stage == STAGE_READY || g_stage == STAGE_FAILED) return;
    // Acquire the mutex: blocks until the task releases it at the end of
    // bringUpTask(). That means "bring-up is done" by the time we get it.
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        xSemaphoreGive(g_mutex);
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
