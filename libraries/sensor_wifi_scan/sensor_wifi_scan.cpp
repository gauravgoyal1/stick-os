#include <M5StickCPlus2.h>
#include <stick_os.h>
#include <stick_net.h>
#include "sensor_wifi_scan.h"

namespace SensorWiFiScan {

constexpr size_t kMaxResults = 16;
constexpr int kRowsPerPage = 8;

static StickNet::ScanResult g_results[kMaxResults];
static size_t g_count = 0;
static int g_page = 0;

void drawHeader(const char* status) {
    auto& d = StickCP2.Display;
    d.fillRect(0, 0, d.width(), 30, BLACK);
    d.setTextSize(2);
    d.setTextColor(YELLOW, BLACK);
    d.setCursor(8, 6);
    d.print("WiFi Scan");
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(8, 24);
    d.print(status);
    d.drawFastHLine(0, 34, d.width(), d.color565(40, 40, 40));
}

void drawPage() {
    auto& d = StickCP2.Display;
    d.fillRect(0, 36, d.width(), d.height() - 50, BLACK);
    d.setTextSize(1);

    if (g_count == 0) {
        d.setTextColor(d.color565(120, 120, 120), BLACK);
        d.setCursor(20, 80);
        d.print("No networks found");
        return;
    }

    int totalPages = (g_count + kRowsPerPage - 1) / kRowsPerPage;
    if (g_page >= totalPages) g_page = 0;
    int start = g_page * kRowsPerPage;

    int y = 38;
    for (int i = start; i < (int)g_count && i < start + kRowsPerPage; i++) {
        const StickNet::ScanResult& r = g_results[i];
        uint16_t color = r.rssi > -60 ? GREEN : r.rssi > -75 ? YELLOW : RED;
        if (r.known) color = CYAN;

        d.setTextColor(color, BLACK);
        d.setCursor(4, y);

        char ssid[17];
        strncpy(ssid, r.ssid, 16);
        ssid[16] = '\0';
        d.printf("%-16s %d", ssid, r.rssi);

        if (r.known) {
            d.setCursor(d.width() - 8, y);
            d.print("*");
        }
        y += 14;
    }

    // Footer
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(4, d.height() - 12);
    d.printf("A:scan B:pg(%d/%d) PWR:back", g_page + 1, totalPages);
}

void runScan() {
    drawHeader("Scanning...");
    g_count = StickNet::scanNetworks(g_results, kMaxResults);
    g_page = 0;
    char status[24];
    snprintf(status, sizeof(status), "%u networks", (unsigned)g_count);
    drawHeader(status);
    drawPage();
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);
    runScan();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnA.wasPressed()) {
            runScan();
        }
        if (StickCP2.BtnB.wasPressed() && g_count > (size_t)kRowsPerPage) {
            int totalPages = (g_count + kRowsPerPage - 1) / kRowsPerPage;
            g_page = (g_page + 1) % totalPages;
            drawPage();
        }
        delay(20);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // WiFi waves
    d.drawArc(x + 14, y + 20, 16, 14, 220, 320, color);
    d.drawArc(x + 14, y + 20, 11, 9, 220, 320, color);
    d.drawArc(x + 14, y + 20, 6, 4, 220, 320, color);
    d.fillCircle(x + 14, y + 20, 2, color);
}

}  // namespace SensorWiFiScan

static const stick_os::AppDescriptor kDesc = {
    "wifi_scan", "Scan", "1.0.0",
    stick_os::CAT_SENSOR, stick_os::APP_NEEDS_NET,
    &SensorWiFiScan::icon, stick_os::RUNTIME_NATIVE,
    { &SensorWiFiScan::init, &SensorWiFiScan::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
