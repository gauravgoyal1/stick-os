#include <M5StickCPlus2.h>
#include <stick_os.h>
#include <stick_net.h>
#include "settings_about.h"

namespace SettingsAbout {

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);  // Portrait 135×240
    d.fillScreen(BLACK);

    const uint16_t gray  = d.color565(140, 140, 140);
    const uint16_t dim   = d.color565(80, 80, 80);
    const uint16_t accent = d.color565(255, 165, 0);  // orange

    int y = 8;

    // Title
    d.setTextSize(2);
    d.setTextColor(accent, BLACK);
    d.setCursor(8, y);
    d.print("About");
    y += 28;
    d.drawFastHLine(0, y, d.width(), d.color565(40, 40, 40));
    y += 8;

    d.setTextSize(1);

    // Firmware
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Firmware");
    y += 12;
    d.setTextColor(gray, BLACK);
    d.setCursor(8, y); d.print("Stick OS v1.0.0");
    y += 18;

    // Build
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Built");
    y += 12;
    d.setTextColor(gray, BLACK);
    d.setCursor(8, y); d.printf("%s %s", __DATE__, __TIME__);
    y += 18;

    // Chip
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Chip");
    y += 12;
    d.setTextColor(gray, BLACK);
    d.setCursor(8, y); d.printf("ESP32 rev%d  %dMHz", ESP.getChipRevision(), ESP.getCpuFreqMHz());
    y += 18;

    // Flash
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Flash");
    y += 12;
    d.setTextColor(gray, BLACK);
    uint32_t flashMB = ESP.getFlashChipSize() / (1024 * 1024);
    uint32_t sketchKB = ESP.getSketchSize() / 1024;
    uint32_t freeKB = ESP.getFreeSketchSpace() / 1024;
    d.setCursor(8, y); d.printf("%luMB total  %luKB used", flashMB, sketchKB);
    y += 10;
    d.setCursor(8, y); d.printf("%luKB free", freeKB);
    y += 18;

    // Heap
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Heap");
    y += 12;
    d.setTextColor(gray, BLACK);
    d.setCursor(8, y); d.printf("%lu / %lu bytes free",
        (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getHeapSize());
    y += 18;

    // WiFi
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("WiFi");
    y += 12;
    d.setTextColor(gray, BLACK);
    if (StickNet::isConnected()) {
        d.setCursor(8, y); d.printf("%s  %ddBm", StickNet::ssid(), StickNet::rssi());
    } else {
        d.setCursor(8, y); d.print("Not connected");
    }
    y += 18;

    // Uptime
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Uptime");
    y += 12;
    d.setTextColor(gray, BLACK);
    unsigned long sec = millis() / 1000;
    unsigned long m = (sec / 60) % 60;
    unsigned long h = sec / 3600;
    d.setCursor(8, y); d.printf("%luh %lum %lus", h, m, sec % 60);

    // Footer
    d.setTextColor(dim, BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    // Wait for exit
    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;
        delay(50);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // "i" info icon: circle + dot + vertical line
    d.drawCircle(x + 14, y + 14, 12, color);
    d.fillRect(x + 13, y + 8, 3, 3, color);
    d.fillRect(x + 13, y + 13, 3, 8, color);
}

}  // namespace SettingsAbout

static const stick_os::AppDescriptor kDesc = {
    "about", "About", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED,
    &SettingsAbout::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsAbout::init, &SettingsAbout::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
