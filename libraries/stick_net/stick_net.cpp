#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <wifi_config.h>
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

    int n = WiFi.scanNetworks();
    if (n <= 0) {
        Serial.println("[StickNet] no networks visible");
        return false;
    }
    Serial.printf("[StickNet] %d networks visible\n", n);

    for (int i = 0; i < n; i++) {
        String found = WiFi.SSID(i);
        for (size_t j = 0; j < kWiFiNetworkCount; j++) {
            if (found != kWiFiNetworks[j].ssid) continue;

            Serial.printf("[StickNet] connecting to %s\n", kWiFiNetworks[j].ssid);
            WiFi.begin(kWiFiNetworks[j].ssid, kWiFiNetworks[j].password);

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                vTaskDelay(pdMS_TO_TICKS(500));
                attempts++;
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[StickNet] connected, ip=%s\n",
                              WiFi.localIP().toString().c_str());
                return true;
            }
            Serial.println("[StickNet] connect failed, trying next");
        }
    }
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

}  // namespace StickNet
