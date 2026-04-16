#include <M5StickCPlus2.h>
#include <stick_os.h>
#include <stick_net.h>
#include "settings_time.h"

namespace SettingsTime {

namespace {

const char* dayName(int dow) {
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    return (dow >= 0 && dow < 7) ? days[dow] : "???";
}

const char* monthName(int m) {
    const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
    return (m >= 1 && m <= 12) ? months[m] : "???";
}

void drawClock() {
    auto& d = StickCP2.Display;
    auto dt = StickCP2.Rtc.getDateTime();

    // Clear clock area
    d.fillRect(0, 36, d.width(), 120, BLACK);

    // Large time
    d.setTextSize(3);
    d.setTextColor(WHITE, BLACK);
    d.setCursor(12, 50);
    d.printf("%02d:%02d", dt.time.hours, dt.time.minutes);

    // Seconds
    d.setTextSize(2);
    d.setTextColor(d.color565(100, 100, 100), BLACK);
    d.setCursor(102, 56);
    d.printf("%02d", dt.time.seconds);

    // Date
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(12, 90);
    d.printf("%s, %02d %s %04d",
        dayName(dt.date.weekDay),
        dt.date.date,
        monthName(dt.date.month),
        dt.date.year);

    // NTP status
    d.setCursor(12, 110);
    StickNet::Stage s = StickNet::status();
    if (s == StickNet::STAGE_READY) {
        d.setTextColor(GREEN, BLACK);
        d.print("NTP synced        ");
    } else if (s == StickNet::STAGE_FAILED) {
        d.setTextColor(RED, BLACK);
        d.print("NTP failed        ");
    } else {
        d.setTextColor(YELLOW, BLACK);
        d.print("NTP pending       ");
    }

    // WiFi
    d.setCursor(12, 125);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    if (StickNet::isConnected()) {
        d.printf("WiFi: %s  %ddBm  ", StickNet::ssid(), StickNet::rssi());
    } else {
        d.print("WiFi: not connected      ");
    }
}

}  // namespace

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    // Title
    d.setTextSize(2);
    d.setTextColor(d.color565(255, 165, 0), BLACK);
    d.setCursor(8, 8);
    d.print("Time");
    d.drawFastHLine(0, 30, d.width(), d.color565(40, 40, 40));

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 24);
    d.print("A: resync NTP");
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    // Initial draw
    drawClock();

    unsigned long lastUpdate = 0;
    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnA.wasPressed()) {
            auto& dd = StickCP2.Display;
            dd.fillRect(12, 110, 120, 10, BLACK);
            dd.setTextSize(1);
            dd.setTextColor(YELLOW, BLACK);
            dd.setCursor(12, 110);
            dd.print("Syncing NTP...");

            StickNet::syncNTP();
            drawClock();
        }

        // Update display every second
        unsigned long now = millis();
        if (now - lastUpdate >= 1000) {
            drawClock();
            lastUpdate = now;
        }

        delay(20);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Clock face
    d.drawCircle(x + 14, y + 14, 12, color);
    // Hour hand
    d.drawLine(x + 14, y + 14, x + 14, y + 6, color);
    // Minute hand
    d.drawLine(x + 14, y + 14, x + 22, y + 14, color);
    // Center dot
    d.fillCircle(x + 14, y + 14, 2, color);
}

}  // namespace SettingsTime

static const stick_os::AppDescriptor kDesc = {
    "time", "Time", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED,
    &SettingsTime::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsTime::init, &SettingsTime::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
