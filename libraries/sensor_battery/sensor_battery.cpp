#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "sensor_battery.h"

namespace SensorBattery {

// Sparkline history
constexpr int kHistLen = 120;  // ~60 seconds at 500ms interval
static uint8_t g_hist[kHistLen] = {0};
static int g_histIdx = 0;
static bool g_histFull = false;

void drawBatteryBar(int pct, bool charging) {
    auto& d = StickCP2.Display;
    const int barX = 14, barY = 50, barW = 107, barH = 24;

    // Outline
    d.drawRect(barX, barY, barW, barH, d.color565(100, 100, 100));
    d.fillRect(barX + barW, barY + 6, 4, 12, d.color565(100, 100, 100));

    // Fill
    int fillW = (pct * (barW - 4)) / 100;
    uint16_t fillColor;
    if (charging)       fillColor = d.color565(100, 200, 255);
    else if (pct > 50)  fillColor = GREEN;
    else if (pct > 20)  fillColor = YELLOW;
    else                fillColor = RED;

    d.fillRect(barX + 2, barY + 2, fillW, barH - 4, fillColor);
    d.fillRect(barX + 2 + fillW, barY + 2, barW - 4 - fillW, barH - 4, BLACK);

    // Percent text centered in bar
    d.setTextSize(2);
    d.setTextColor(WHITE, fillW > barW / 2 ? fillColor : BLACK);
    d.setCursor(barX + barW / 2 - 18, barY + 5);
    d.printf("%d%%", pct);

    // Charging label
    d.setTextSize(1);
    d.setCursor(barX, barY + barH + 6);
    if (charging) {
        d.setTextColor(d.color565(100, 200, 255), BLACK);
        d.print("Charging     ");
    } else {
        d.setTextColor(d.color565(80, 80, 80), BLACK);
        d.print("On battery   ");
    }
}

void drawSparkline() {
    auto& d = StickCP2.Display;
    const int sx = 8, sy = 110, sw = 120, sh = 60;

    // Background + border
    d.fillRect(sx, sy, sw, sh, BLACK);
    d.drawRect(sx, sy, sw, sh, d.color565(40, 40, 40));

    // Grid lines at 25%, 50%, 75%
    for (int p = 25; p < 100; p += 25) {
        int gy = sy + sh - 1 - (p * (sh - 2)) / 100;
        for (int gx = sx + 1; gx < sx + sw - 1; gx += 4) {
            d.drawPixel(gx, gy, d.color565(30, 30, 30));
        }
    }

    // Plot
    int count = g_histFull ? kHistLen : g_histIdx;
    if (count < 2) return;

    for (int i = 1; i < count && i < sw - 2; i++) {
        int idx0 = (g_histIdx - count + i - 1 + kHistLen) % kHistLen;
        int idx1 = (g_histIdx - count + i + kHistLen) % kHistLen;
        int x0 = sx + 1 + (i - 1) * (sw - 2) / (count - 1);
        int x1 = sx + 1 + i * (sw - 2) / (count - 1);
        int y0 = sy + sh - 2 - (g_hist[idx0] * (sh - 4)) / 100;
        int y1 = sy + sh - 2 - (g_hist[idx1] * (sh - 4)) / 100;
        d.drawLine(x0, y0, x1, y1, GREEN);
    }

    // Label
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(sx, sy + sh + 4);
    d.print("History (~60s)");
}

void drawDetails() {
    auto& d = StickCP2.Display;
    int y = 190;
    d.setTextSize(1);

    int mv = StickCP2.Power.getBatteryVoltage();
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(8, y);
    d.printf("Voltage: %d.%03dV   ", mv / 1000, mv % 1000);
    y += 14;

    d.setCursor(8, y);
    d.printf("Heap: %luK free   ", (unsigned long)(ESP.getFreeHeap() / 1024));
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    // Reset history
    memset(g_hist, 0, sizeof(g_hist));
    g_histIdx = 0;
    g_histFull = false;

    // Title
    d.setTextSize(2);
    d.setTextColor(YELLOW, BLACK);
    d.setCursor(8, 8);
    d.print("Battery");
    d.drawFastHLine(0, 34, d.width(), d.color565(40, 40, 40));

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    // Live update loop
    unsigned long lastUpdate = 0;
    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        unsigned long now = millis();
        if (now - lastUpdate >= 500) {
            int pct = StickCP2.Power.getBatteryLevel();
            bool charging = StickCP2.Power.isCharging();

            // Record history
            g_hist[g_histIdx] = (uint8_t)pct;
            g_histIdx = (g_histIdx + 1) % kHistLen;
            if (g_histIdx == 0) g_histFull = true;

            drawBatteryBar(pct, charging);
            drawSparkline();
            drawDetails();
            lastUpdate = now;
        }
        delay(20);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Battery outline
    d.drawRect(x + 2, y + 4, 20, 12, color);
    d.fillRect(x + 22, y + 7, 3, 6, color);
    // Fill bars
    d.fillRect(x + 4, y + 6, 5, 8, color);
    d.fillRect(x + 10, y + 6, 5, 8, color);
}

}  // namespace SensorBattery

static const stick_os::AppDescriptor kDesc = {
    "battery", "Battery", "1.0.0",
    stick_os::CAT_SENSOR, stick_os::APP_NONE,
    &SensorBattery::icon, stick_os::RUNTIME_NATIVE,
    { &SensorBattery::init, &SensorBattery::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
