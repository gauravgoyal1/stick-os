#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "settings_storage.h"

namespace SettingsStorage {

void drawBar(int x, int y, int w, int h, uint32_t used, uint32_t total, uint16_t color, const char* label) {
    auto& d = StickCP2.Display;
    d.drawRect(x, y, w, h, d.color565(80, 80, 80));
    int fillW = (total > 0) ? (int)((uint64_t)used * (w - 2) / total) : 0;
    d.fillRect(x + 1, y + 1, fillW, h - 2, color);
    d.fillRect(x + 1 + fillW, y + 1, w - 2 - fillW, h - 2, d.color565(20, 20, 20));
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(x, y + h + 4);
    d.print(label);
    d.setCursor(x, y + h + 16);
    d.setTextColor(d.color565(100, 100, 100), BLACK);
    d.printf("%luK / %luK", (unsigned long)(used / 1024), (unsigned long)(total / 1024));
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    int y = 8;

    // Title
    d.setTextSize(2);
    d.setTextColor(d.color565(255, 165, 0), BLACK);
    d.setCursor(8, y);
    d.print("Storage");
    y += 28;
    d.drawFastHLine(0, y, d.width(), d.color565(40, 40, 40));
    y += 12;

    // Flash
    uint32_t sketchSize = ESP.getSketchSize();
    uint32_t flashTotal = ESP.getFlashChipSize();
    drawBar(8, y, d.width() - 16, 20, sketchSize, flashTotal, d.color565(0, 120, 255), "Flash (firmware)");
    y += 50;

    // Heap
    uint32_t heapUsed = ESP.getHeapSize() - ESP.getFreeHeap();
    uint32_t heapTotal = ESP.getHeapSize();
    drawBar(8, y, d.width() - 16, 20, heapUsed, heapTotal, GREEN, "RAM (heap)");
    y += 50;

    // LittleFS (app storage)
    if (stick_os::fsReady()) {
        uint32_t fsUsed  = stick_os::fsUsedBytes();
        uint32_t fsTotal = stick_os::fsTotalBytes();
        drawBar(8, y, d.width() - 16, 20, fsUsed, fsTotal,
                d.color565(200, 100, 255), "Apps (LittleFS)");
        y += 50;
    }

    // Summary
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, y);
    d.printf("Apps registered: %u", (unsigned)stick_os::appCount());
    y += 14;
    d.setCursor(8, y);
    d.printf("Flash total: %lu MB", (unsigned long)(flashTotal / (1024 * 1024)));

    // Footer
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;
        delay(50);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Hard drive icon: rounded rect + line
    d.drawRoundRect(x + 2, y + 4, 24, 16, 3, color);
    d.drawFastHLine(x + 2, y + 14, 24, color);
    d.fillCircle(x + 21, y + 17, 2, color);
}

}  // namespace SettingsStorage

static const stick_os::AppDescriptor kDesc = {
    "storage", "Storage", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED,
    &SettingsStorage::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsStorage::init, &SettingsStorage::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
