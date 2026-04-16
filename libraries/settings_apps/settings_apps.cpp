#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "settings_apps.h"

namespace SettingsApps {

static const char* catLabel(stick_os::AppCategory c) {
    switch (c) {
        case stick_os::CAT_GAME:     return "Game";
        case stick_os::CAT_UTILITY:  return "App";
        case stick_os::CAT_SENSOR:   return "Sensor";
        case stick_os::CAT_SETTINGS: return "System";
        default:                     return "?";
    }
}

static const uint16_t catColors[] = { 0x07E0, 0x07FF, 0xFFE0, 0xFD20 };

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    int y = 8;

    // Title
    d.setTextSize(2);
    d.setTextColor(d.color565(255, 165, 0), BLACK);
    d.setCursor(8, y);
    d.print("Apps");
    d.setTextSize(1);
    d.setTextColor(d.color565(100, 100, 100), BLACK);
    d.setCursor(64, y + 6);
    d.printf("(%u total)", (unsigned)stick_os::appCount());
    y += 28;
    d.drawFastHLine(0, y, d.width(), d.color565(40, 40, 40));
    y += 6;

    // Scrollable list -- show all apps
    size_t total = stick_os::appCount();
    int page = 0;
    const int rowH = 14;
    const int maxRows = 13;

    auto drawPage = [&]() {
        d.fillRect(0, 42, d.width(), d.height() - 54, BLACK);
        int py = 42;
        int start = page * maxRows;
        for (int i = start; i < (int)total && i < start + maxRows; i++) {
            const stick_os::AppDescriptor* app = stick_os::appAt(i);
            if (!app) continue;
            // Color dot for category
            d.fillCircle(8, py + 3, 3, catColors[app->category]);
            // App name
            d.setTextColor(d.color565(140, 140, 140), BLACK);
            d.setCursor(16, py);
            d.print(app->name);
            py += rowH;
        }
        int totalPages = (total + maxRows - 1) / maxRows;
        d.setTextColor(d.color565(80, 80, 80), BLACK);
        d.setCursor(4, d.height() - 12);
        d.printf("B:pg(%d/%d)  PWR:back", page + 1, (int)totalPages);
    };

    drawPage();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnB.wasPressed() && total > (size_t)maxRows) {
            int totalPages = (total + maxRows - 1) / maxRows;
            page = (page + 1) % totalPages;
            drawPage();
        }
        delay(20);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Grid of 4 small squares
    d.fillRect(x + 4, y + 4, 8, 8, color);
    d.fillRect(x + 16, y + 4, 8, 8, color);
    d.fillRect(x + 4, y + 16, 8, 8, color);
    d.fillRect(x + 16, y + 16, 8, 8, color);
}

}  // namespace SettingsApps

static const stick_os::AppDescriptor kDesc = {
    "apps", "Installed", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED,
    &SettingsApps::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsApps::init, &SettingsApps::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
