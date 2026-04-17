// Stick OS — registry-driven launcher for M5StickC Plus 2.
//
// stick_os enumerates AppDescriptors registered by app libraries via
// STICK_REGISTER_APP. Two-level nav: category picker -> app list ->
// running app. Short PWR-click pops one level up.
//
// Launcher logic is split across:
//   launcher_state.h  — shared state, constants, declarations
//   launcher_draw.cpp — category picker & app list rendering
//   launcher_nav.cpp  — persistence, state transitions, tick handlers
//   serial_cmd.cpp    — USB serial provisioning commands

#include <M5StickCPlus2.h>
#include <stick_net.h>
#include <stick_os.h>

#include "launcher_state.h"

// App library includes — arduino-cli compiles libraries only when their
// headers appear in the sketch's include tree. These includes exist
// solely to trigger that discovery; the launcher uses the stick_os
// registry at runtime, not these headers directly.
#include <arcade_common.h>
#include <game_flappy.h>
#include <game_dino.h>
#include <game_scream.h>
#include <game_galaxy.h>
#include <game_balance.h>
#include <game_simon.h>
#include <game_panic.h>
#include <scribe.h>
#include <settings_about.h>
#include <sensor_battery.h>
#include <sensor_wifi_scan.h>
#include <sensor_imu.h>
#include <sensor_mic.h>
#include <app_stopwatch.h>
#include <app_flashlight.h>
#include <settings_wifi.h>
#include <settings_storage.h>
#include <settings_apps.h>
#include <settings_time.h>
#include <settings_update.h>

void setup() {
    StickCP2.begin();
    Serial.begin(115200);
    delay(50);
    Serial.println("stick booted");
    Serial.printf("[stick_os] %u apps registered\n",
                  (unsigned)stick_os::appCount());
    for (size_t i = 0; i < stick_os::appCount(); i++) {
        auto* d = stick_os::appAt(i);
        Serial.printf("  [%u] %s (cat=%u)\n", (unsigned)i, d->name,
                      d->category);
    }

    // WiFi: auto-connect to known networks, fall back to picker
    {
        StickCP2.Display.setRotation(0);
        StickCP2.Display.fillScreen(BLACK);
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(YELLOW, BLACK);
        StickCP2.Display.setCursor(20, 100);
        StickCP2.Display.print("Connecting...");

        StickNet::startAsync();
        StickNet::waitForReady(20000);
        bool connected = StickNet::isConnected();

        if (!connected) {
            connected = stick_os::showWiFiPicker();
            if (connected) {
                StickNet::syncNTP();
            }
        }

        if (!connected) {
            Serial.println("[stick] no WiFi — proceeding offline");
        }
    }

    stick_os::fsInit();

    // ---- Phase 2e: scan LittleFS for installed scripted apps ----
    // Each /apps/<id>/manifest.json produces a heap-allocated
    // AppDescriptor registered into the runtime registry. Categories
    // are clamped to CAT_GAME or CAT_UTILITY by registerApp().
    size_t mpyCount = stick_os::scanInstalledApps();
    Serial.printf("[stick] %u scripted app(s) loaded from LittleFS\n",
                  (unsigned)mpyCount);

    stick_os::statusStripInit();

    launcher::enterCategories();
}

void loop() {
    using namespace launcher;

    StickCP2.update();
    processSerialCommand();

    if (g_btnDrainFrames > 0) { g_btnDrainFrames--; delay(10); return; }

    // PWR-click: pop one level up.
    if (M5.BtnPWR.wasClicked()) {
        switch (g_state) {
            case ST_APP:
                Serial.println("[stick] pwr: app -> list");
                g_runningApp = nullptr;
                enterAppList(g_openCategory);
                return;
            case ST_APP_LIST:
                Serial.println("[stick] pwr: list -> categories");
                enterCategories();
                return;
            case ST_CATEGORIES:
            default:
                break;
        }
    }

    // Running app: forward the tick. Check exit flag after.
    if (g_state == ST_APP) {
        if (g_runningApp && g_runningApp->native.tick) {
            g_runningApp->native.tick();
        }
        if (stick_os::wasExitRequested()) {
            stick_os::clearExitRequest();
            stick_os::logHeap("exit");
            g_runningApp = nullptr;
            enterAppList(g_openCategory);
        }
        // Update status strip for portrait-mode apps
        if (g_runningApp && StickCP2.Display.getRotation() == 0) {
            const uint32_t now = millis();
            if (now - g_lastStripTick > 500) {
                stick_os::statusStripTick(g_runningApp->name);
                g_lastStripTick = now;
            }
        }
        return;
    }

    // Launcher: handle input then repaint strip periodically.
    if (g_state == ST_CATEGORIES) tickCategories();
    else                          tickAppList();

    const uint32_t now = millis();
    if (now - g_lastStripTick > 500) {
        const char* label = (g_state == ST_CATEGORIES)
            ? "Stick" : kCategoryNames[g_openCategory];
        stick_os::statusStripTick(label);
        g_lastStripTick = now;
    }
    delay(20);
}
