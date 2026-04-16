#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "app_flashlight.h"

namespace AppFlashlight {

static const uint16_t kColors[] = {
    WHITE,
    0xFEF3,  // warm white (slightly yellow tint)
    RED,
    GREEN,
    BLUE,
};
static const char* const kColorNames[] = {
    "White", "Warm", "Red", "Green", "Blue",
};
constexpr int kColorCount = sizeof(kColors) / sizeof(kColors[0]);

static int g_colorIdx = 0;
static bool g_irOn = false;

void fillScreen() {
    auto& d = StickCP2.Display;
    d.fillScreen(kColors[g_colorIdx]);

    // Show IR status at bottom in contrasting color
    uint16_t textColor = (kColors[g_colorIdx] == WHITE || kColors[g_colorIdx] == 0xFEF3)
                         ? BLACK : WHITE;
    d.setTextSize(1);
    d.setTextColor(textColor, kColors[g_colorIdx]);
    d.setCursor(4, d.height() - 12);
    d.printf("IR: %s  [%s]", g_irOn ? "ON " : "OFF", kColorNames[g_colorIdx]);
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);

    g_colorIdx = 0;
    g_irOn = false;

    // Save and set max brightness
    uint8_t prevBrightness = d.getBrightness();
    d.setBrightness(255);

    // IR LED setup
    pinMode(19, OUTPUT);
    digitalWrite(19, LOW);

    fillScreen();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) {
            // Restore state
            d.setBrightness(prevBrightness);
            digitalWrite(19, LOW);
            return;
        }

        if (StickCP2.BtnA.wasPressed()) {
            g_colorIdx = (g_colorIdx + 1) % kColorCount;
            fillScreen();
        }

        if (StickCP2.BtnB.wasPressed()) {
            g_irOn = !g_irOn;
            digitalWrite(19, g_irOn ? HIGH : LOW);
            fillScreen();
        }

        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Filled circle (bulb)
    d.fillCircle(x + 14, y + 12, 7, color);
    // Rays
    d.drawLine(x + 14, y + 1, x + 14, y + 4, color);    // top
    d.drawLine(x + 5,  y + 12, x + 2,  y + 12, color);  // left
    d.drawLine(x + 23, y + 12, x + 26, y + 12, color);  // right
    d.drawLine(x + 8,  y + 5,  x + 5,  y + 3, color);   // top-left
    d.drawLine(x + 20, y + 5,  x + 23, y + 3, color);   // top-right
}

}  // namespace AppFlashlight

static const stick_os::AppDescriptor kDesc = {
    "flashlight", "Light", "1.0.0",
    stick_os::CAT_UTILITY, stick_os::APP_NEEDS_IR,
    &AppFlashlight::icon, stick_os::RUNTIME_NATIVE,
    { &AppFlashlight::init, &AppFlashlight::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
