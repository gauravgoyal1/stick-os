#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "app_stopwatch.h"

namespace AppStopwatch {

static bool g_running = false;
static unsigned long g_elapsed = 0;   // accumulated ms
static unsigned long g_startAt = 0;   // millis() when last started
static int g_laps = 0;
static bool g_justReset = false;

unsigned long totalElapsed() {
    if (g_running) return g_elapsed + (millis() - g_startAt);
    return g_elapsed;
}

void drawTime() {
    auto& d = StickCP2.Display;
    unsigned long ms = totalElapsed();
    unsigned long totalSec = ms / 1000;
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;
    int centis  = (ms % 1000) / 10;

    uint16_t color;
    if (g_justReset)  color = RED;
    else if (g_running) color = GREEN;
    else              color = WHITE;

    // MM:SS
    d.setTextSize(3);
    d.setTextColor(color, BLACK);
    d.setCursor(10, 60);
    d.printf("%02d:%02d", minutes, seconds);

    // .ms
    d.setTextSize(2);
    d.setCursor(100, 66);
    d.printf(".%02d", centis);

    g_justReset = false;
}

void drawStatus() {
    auto& d = StickCP2.Display;

    // Lap count
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(10, 100);
    d.printf("Laps: %d   ", g_laps);

    // State indicator
    d.setCursor(10, 116);
    if (g_running) {
        d.setTextColor(GREEN, BLACK);
        d.print("RUNNING  ");
    } else {
        d.setTextColor(d.color565(100, 100, 100), BLACK);
        d.print("STOPPED  ");
    }
}

void drawControls() {
    auto& d = StickCP2.Display;
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 24);
    d.print("A:start/stop");
    d.setCursor(8, d.height() - 12);
    d.print("B:reset  PWR:back");
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    g_running = false;
    g_elapsed = 0;
    g_startAt = 0;
    g_laps = 0;
    g_justReset = false;

    // Title
    d.setTextSize(2);
    d.setTextColor(CYAN, BLACK);
    d.setCursor(8, 8);
    d.print("Stopwatch");
    d.drawFastHLine(0, 30, d.width(), d.color565(40, 40, 40));

    drawControls();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnA.wasPressed()) {
            if (g_running) {
                // Stop
                g_elapsed += (millis() - g_startAt);
                g_running = false;
                g_laps++;
            } else {
                // Start
                g_startAt = millis();
                g_running = true;
            }
        }

        if (StickCP2.BtnB.wasPressed() && !g_running) {
            g_elapsed = 0;
            g_laps = 0;
            g_justReset = true;
        }

        drawTime();
        drawStatus();
        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Circle (watch body)
    d.drawCircle(x + 14, y + 14, 11, color);
    // Line pointing up-right (hand)
    d.drawLine(x + 14, y + 14, x + 20, y + 6, color);
    // Button on top
    d.fillRect(x + 12, y + 1, 4, 3, color);
}

}  // namespace AppStopwatch

static const stick_os::AppDescriptor kDesc = {
    "stopwatch", "Timer", "1.0.0",
    stick_os::CAT_UTILITY, stick_os::APP_NONE,
    &AppStopwatch::icon, stick_os::RUNTIME_NATIVE,
    { &AppStopwatch::init, &AppStopwatch::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
