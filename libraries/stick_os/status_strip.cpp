#include "status_strip.h"

#include <M5StickCPlus2.h>

#include <stick_net.h>

namespace stick_os {

namespace {

int  g_lastRssiBars     = -1;
bool g_lastConnected    = false;
int  g_lastMinute       = -1;
int  g_lastBatteryPct   = -1;
StickNet::Stage g_lastStage = StickNet::STAGE_IDLE;
bool g_dirty            = true;

void drawBars(int x, int y, int bars) {
    auto& d = StickCP2.Display;
    const int heights[] = {3, 5, 7, 9};
    const uint16_t dim  = d.color565(50, 50, 50);
    for (int i = 0; i < 4; i++) {
        int bx = x + i * 3;
        int by = y + 9 - heights[i];
        uint16_t col = (i < bars) ? GREEN : dim;
        d.fillRect(bx, by, 2, heights[i], col);
    }
}

int barsFromRssi(int rssi, bool connected) {
    if (!connected) return 0;
    if (rssi >= -50) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -80) return 2;
    return 1;
}

}  // namespace

void statusStripInit() {
    g_lastRssiBars   = -1;
    g_lastConnected  = false;
    g_lastMinute     = -1;
    g_lastBatteryPct = -1;
    g_lastStage      = StickNet::STAGE_IDLE;
    g_dirty          = true;
}

void statusStripDrawFull() {
    auto& d = StickCP2.Display;
    d.fillRect(0, 0, d.width(), kStatusStripHeight, BLACK);
    d.drawFastHLine(0, kStatusStripHeight - 1, d.width(), d.color565(40, 40, 40));
    g_dirty = true;
}

void statusStripTick(const char* appName) {
    auto& d = StickCP2.Display;

    if (g_dirty) {
        g_lastRssiBars   = -1;
        g_lastConnected  = !StickNet::isConnected();
        g_lastMinute     = -1;
        g_lastBatteryPct = -1;
        g_lastStage      = static_cast<StickNet::Stage>(255);
        g_dirty          = false;
    }

    // Left: app name or blank.
    d.fillRect(0, 0, 120, kStatusStripHeight - 1, BLACK);
    if (appName != nullptr) {
        d.setTextSize(1);
        d.setTextColor(d.color565(180, 180, 180), BLACK);
        d.setCursor(4, 5);
        d.print(appName);
    }

    // Right: status block. Layout from right-to-left: bars, clock, battery%.
    const int w = d.width();

    // WiFi bars.
    const bool connected = StickNet::isConnected();
    const int  bars      = barsFromRssi(StickNet::rssi(), connected);
    if (bars != g_lastRssiBars || connected != g_lastConnected) {
        d.fillRect(w - 16, 4, 14, 10, BLACK);
        drawBars(w - 16, 4, bars);
        g_lastRssiBars  = bars;
        g_lastConnected = connected;
    }

    // Clock / stage.
    const StickNet::Stage stage = StickNet::status();
    if (stage == StickNet::STAGE_READY) {
        auto dt = StickCP2.Rtc.getDateTime();
        int minute = dt.time.minutes;
        if (minute != g_lastMinute || stage != g_lastStage) {
            d.fillRect(w - 60, 5, 40, 8, BLACK);
            d.setTextSize(1);
            d.setTextColor(GREEN, BLACK);
            d.setCursor(w - 58, 5);
            d.printf("%02d:%02d", dt.time.hours, minute);
            g_lastMinute = minute;
        }
    } else if (stage != g_lastStage) {
        d.fillRect(w - 60, 5, 40, 8, BLACK);
        d.setTextSize(1);
        d.setCursor(w - 58, 5);
        switch (stage) {
            case StickNet::STAGE_WIFI:   d.setTextColor(YELLOW, BLACK); d.print("WiFi."); break;
            case StickNet::STAGE_NTP:    d.setTextColor(YELLOW, BLACK); d.print("NTP.");  break;
            case StickNet::STAGE_FAILED: d.setTextColor(RED, BLACK);    d.print("!net");  break;
            default:                     break;
        }
    }
    g_lastStage = stage;

    // Battery percent.
    int pct = StickCP2.Power.getBatteryLevel();
    if (pct != g_lastBatteryPct) {
        d.fillRect(w - 90, 5, 28, 8, BLACK);
        d.setTextSize(1);
        d.setTextColor(pct < 20 ? RED : d.color565(180, 180, 180), BLACK);
        d.setCursor(w - 88, 5);
        d.printf("%d%%", pct);
        g_lastBatteryPct = pct;
    }
}

}  // namespace stick_os
