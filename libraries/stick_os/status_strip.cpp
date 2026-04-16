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

    // Force full repaint every tick while an app is running,
    // since games may overdraw the status strip area.
    if (appName != nullptr) g_dirty = true;

    if (g_dirty) {
        g_lastRssiBars   = -1;
        g_lastConnected  = !StickNet::isConnected();
        g_lastMinute     = -1;
        g_lastBatteryPct = -1;
        g_lastStage      = static_cast<StickNet::Stage>(255);
        g_dirty          = false;
    }

    // Layout (left to right): label | gap | WiFi bars | battery% | time
    const int w = d.width();

    // Left: label.
    d.fillRect(0, 0, 42, kStatusStripHeight - 1, BLACK);
    if (appName != nullptr) {
        d.setTextSize(1);
        d.setTextColor(d.color565(180, 180, 180), BLACK);
        d.setCursor(4, 5);
        d.print(appName);
    }

    // WiFi bars — after the label gap.
    const bool connected = StickNet::isConnected();
    const int  bars      = barsFromRssi(StickNet::rssi(), connected);
    const int  wifiX     = w - 80;
    if (bars != g_lastRssiBars || connected != g_lastConnected) {
        d.fillRect(wifiX, 4, 14, 10, BLACK);
        drawBars(wifiX, 4, bars);
        g_lastRssiBars  = bars;
        g_lastConnected = connected;
    }

    // Battery icon — after WiFi bars.
    const int battX = w - 58;
    int pct = StickCP2.Power.getBatteryLevel();
    bool charging = StickCP2.Power.isCharging();
    if (pct != g_lastBatteryPct || g_dirty) {
        d.fillRect(battX, 3, 18, 12, BLACK);
        // Battery outline: body + nub
        d.drawRect(battX, 4, 14, 9, d.color565(100, 100, 100));
        d.fillRect(battX + 14, 6, 2, 5, d.color565(100, 100, 100));
        // Fill level (1-12 px wide, inside the body)
        int fillW = (pct * 12) / 100;
        if (fillW < 1 && pct > 0) fillW = 1;
        uint16_t fillColor;
        if (charging)       fillColor = d.color565(100, 200, 255);  // light blue
        else if (pct > 50)  fillColor = GREEN;
        else if (pct > 20)  fillColor = YELLOW;
        else                fillColor = RED;
        if (fillW > 0) d.fillRect(battX + 1, 5, fillW, 7, fillColor);
        // Charging bolt overlay
        if (charging) {
            d.drawLine(battX + 8, 5, battX + 5, 8, WHITE);
            d.drawLine(battX + 5, 8, battX + 9, 8, WHITE);
            d.drawLine(battX + 9, 8, battX + 6, 11, WHITE);
        }
        g_lastBatteryPct = pct;
    }

    // Time / stage — rightmost.
    const int timeX = w - 32;
    const StickNet::Stage stage = StickNet::status();
    if (stage == StickNet::STAGE_READY) {
        auto dt = StickCP2.Rtc.getDateTime();
        int minute = dt.time.minutes;
        if (minute != g_lastMinute || stage != g_lastStage) {
            d.fillRect(timeX, 5, 30, 8, BLACK);
            d.setTextSize(1);
            d.setTextColor(GREEN, BLACK);
            d.setCursor(timeX + 2, 5);
            d.printf("%02d:%02d", dt.time.hours, minute);
            g_lastMinute = minute;
        }
    } else if (stage != g_lastStage) {
        d.fillRect(timeX, 5, 30, 8, BLACK);
        d.setTextSize(1);
        d.setCursor(timeX + 2, 5);
        switch (stage) {
            case StickNet::STAGE_WIFI:   d.setTextColor(YELLOW, BLACK); d.print("WiFi"); break;
            case StickNet::STAGE_NTP:    d.setTextColor(YELLOW, BLACK); d.print("NTP");  break;
            case StickNet::STAGE_FAILED: d.setTextColor(RED, BLACK);    d.print("!net"); break;
            default:                     break;
        }
    }
    g_lastStage = stage;
}

}  // namespace stick_os
