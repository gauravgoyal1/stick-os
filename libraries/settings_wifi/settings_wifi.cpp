#include <M5StickCPlus2.h>
#include <stick_os.h>
#include <stick_net.h>
#include <stick_config.h>
#include "settings_wifi.h"

namespace SettingsWiFi {

void drawScreen() {
    auto& d = StickCP2.Display;
    d.fillScreen(BLACK);

    int y = 8;

    // Title
    d.setTextSize(2);
    d.setTextColor(d.color565(255, 165, 0), BLACK);
    d.setCursor(8, y);
    d.print("WiFi");
    y += 28;
    d.drawFastHLine(0, y, d.width(), d.color565(40, 40, 40));
    y += 8;

    d.setTextSize(1);
    const uint16_t gray = d.color565(140, 140, 140);
    const uint16_t dim  = d.color565(80, 80, 80);

    // Connection status
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.print("Status");
    y += 12;
    if (StickNet::isConnected()) {
        d.setTextColor(GREEN, BLACK);
        d.setCursor(8, y); d.print("Connected");
        y += 16;

        // SSID
        d.setTextColor(dim, BLACK);
        d.setCursor(8, y); d.print("SSID");
        y += 12;
        d.setTextColor(gray, BLACK);
        d.setCursor(8, y); d.print(StickNet::ssid());
        y += 16;

        // RSSI
        d.setTextColor(dim, BLACK);
        d.setCursor(8, y); d.print("Signal");
        y += 12;
        int rssi = StickNet::rssi();
        d.setTextColor(rssi > -60 ? GREEN : rssi > -75 ? YELLOW : RED, BLACK);
        d.setCursor(8, y); d.printf("%d dBm", rssi);
        y += 16;
    } else {
        d.setTextColor(RED, BLACK);
        d.setCursor(8, y); d.print("Not connected");
        y += 16;
    }

    // Known networks
    d.setTextColor(dim, BLACK);
    d.setCursor(8, y); d.printf("Known networks (%u)", (unsigned)kWiFiNetworkCount);
    y += 12;
    for (size_t i = 0; i < kWiFiNetworkCount && i < 4; i++) {
        d.setTextColor(gray, BLACK);
        d.setCursor(12, y); d.print(kWiFiNetworks[i].ssid);
        y += 12;
    }

    // Footer
    d.setTextColor(dim, BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("A:reconnect  PWR:back");
}

void init() {
    StickCP2.Display.setRotation(0);
    drawScreen();

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnA.wasPressed()) {
            auto& d = StickCP2.Display;
            d.setTextSize(1);
            d.setTextColor(YELLOW, BLACK);
            d.setCursor(8, 100);
            d.print("Reconnecting...");
            StickNet::connectWiFi();
            drawScreen();
        }

        // Refresh status every 2 seconds
        static unsigned long lastRefresh = 0;
        if (millis() - lastRefresh > 2000) {
            drawScreen();
            lastRefresh = millis();
        }

        delay(20);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    d.drawArc(x + 14, y + 18, 14, 12, 220, 320, color);
    d.drawArc(x + 14, y + 18, 9, 7, 220, 320, color);
    d.fillCircle(x + 14, y + 18, 3, color);
}

}  // namespace SettingsWiFi

static const stick_os::AppDescriptor kDesc = {
    "wifi_config", "WiFi", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED,
    &SettingsWiFi::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsWiFi::init, &SettingsWiFi::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
