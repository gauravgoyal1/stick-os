#include "wifi_picker.h"
#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <stick_net.h>
#include "stick_store.h"
#include "status_strip.h"
#include "app_context.h"

namespace stick_os {

namespace {

struct PickerEntry {
    char ssid[33];
    int8_t rssi;
    bool known;      // in NVS
    bool open;       // WIFI_AUTH_OPEN
    bool locked;     // has password, not in NVS
};

constexpr int kMaxEntries = 16;

void drawPicker(PickerEntry* entries, int count, int cursor) {
    auto& d = StickCP2.Display;
    d.setRotation(0);  // portrait
    d.fillScreen(BLACK);

    d.setTextSize(2);
    d.setTextColor(GREEN, BLACK);
    d.setCursor(8, 6);
    d.print("WiFi");
    d.drawFastHLine(0, 26, d.width(), d.color565(40, 40, 40));

    if (count == 0) {
        d.setTextSize(1);
        d.setTextColor(d.color565(120, 120, 120), BLACK);
        d.setCursor(20, 60);
        d.print("No networks found");
        d.setCursor(20, 80);
        d.print("A:rescan  PWR:skip");
        return;
    }

    const int rowH = 28;
    const int startY = 30;
    const int maxVisible = 7;

    int top = cursor - 3;
    if (top < 0) top = 0;
    if (top > count - maxVisible) top = count - maxVisible;
    if (top < 0) top = 0;

    for (int i = 0; i < maxVisible && (top + i) < count; i++) {
        const PickerEntry& e = entries[top + i];
        const bool sel = (top + i) == cursor;
        const int y = startY + i * rowH;

        uint16_t fg;
        if (e.known)       fg = CYAN;
        else if (e.open)   fg = GREEN;
        else               fg = d.color565(60, 60, 60);  // greyed out

        uint16_t bg = sel ? d.color565(15, 25, 15) : BLACK;

        if (sel) d.fillRect(0, y, d.width(), rowH - 2, bg);

        // SSID
        d.setTextSize(1);
        d.setTextColor(sel ? WHITE : fg, bg);
        d.setCursor(6, y + 4);
        char truncated[18];
        strncpy(truncated, e.ssid, 17);
        truncated[17] = '\0';
        d.print(truncated);

        // Signal + label
        d.setCursor(110, y + 4);
        d.setTextColor(d.color565(80, 80, 80), bg);
        d.printf("%d", e.rssi);

        d.setCursor(6, y + 16);
        d.setTextColor(d.color565(60, 60, 60), bg);
        if (e.known)       d.print("Known");
        else if (e.open)   d.print("Open");
        else               d.print("Locked");
    }

    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(4, d.height() - 12);
    d.print("A:connect B:next PWR:skip");
}

}  // namespace

bool showWiFiPicker() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);
    d.setTextSize(1);
    d.setTextColor(YELLOW, BLACK);
    d.setCursor(20, 100);
    d.print("Scanning WiFi...");

    // Scan
    WiFi.mode(WIFI_STA);
    int scanCount = WiFi.scanNetworks();

    // Load NVS creds for "known" marking
    WiFiCred nvsCreds[kMaxWiFiNetworks];
    size_t nvsCount = loadWiFiCreds(nvsCreds, kMaxWiFiNetworks);

    // Build entry list
    PickerEntry entries[kMaxEntries];
    int count = 0;

    // Known networks first
    for (int i = 0; i < scanCount && count < kMaxEntries; i++) {
        String ssid = WiFi.SSID(i);
        bool isKnown = false;
        for (size_t j = 0; j < nvsCount; j++) {
            if (ssid == nvsCreds[j].ssid) { isKnown = true; break; }
        }
        if (!isKnown) continue;
        PickerEntry& e = entries[count++];
        strncpy(e.ssid, ssid.c_str(), 32); e.ssid[32] = '\0';
        e.rssi = WiFi.RSSI(i);
        e.known = true;
        e.open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        e.locked = false;
    }

    // Open networks next
    for (int i = 0; i < scanCount && count < kMaxEntries; i++) {
        String ssid = WiFi.SSID(i);
        // Skip if already added as known
        bool dup = false;
        for (int j = 0; j < count; j++) {
            if (ssid == entries[j].ssid) { dup = true; break; }
        }
        if (dup) continue;
        if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) continue;
        PickerEntry& e = entries[count++];
        strncpy(e.ssid, ssid.c_str(), 32); e.ssid[32] = '\0';
        e.rssi = WiFi.RSSI(i);
        e.known = false;
        e.open = true;
        e.locked = false;
    }

    // Locked unknown networks last (greyed out)
    for (int i = 0; i < scanCount && count < kMaxEntries; i++) {
        String ssid = WiFi.SSID(i);
        bool dup = false;
        for (int j = 0; j < count; j++) {
            if (ssid == entries[j].ssid) { dup = true; break; }
        }
        if (dup) continue;
        PickerEntry& e = entries[count++];
        strncpy(e.ssid, ssid.c_str(), 32); e.ssid[32] = '\0';
        e.rssi = WiFi.RSSI(i);
        e.known = false;
        e.open = false;
        e.locked = true;
    }

    WiFi.scanDelete();

    int cursor = 0;
    drawPicker(entries, count, cursor);

    while (true) {
        StickCP2.update();
        if (checkAppExit()) return false;

        if (StickCP2.BtnB.wasPressed() && count > 0) {
            cursor = (cursor + 1) % count;
            drawPicker(entries, count, cursor);
        }

        if (StickCP2.BtnA.wasPressed()) {
            if (count == 0) {
                // Rescan
                return showWiFiPicker();
            }

            const PickerEntry& sel = entries[cursor];
            if (sel.locked) continue;  // can't connect to locked unknown

            d.fillScreen(BLACK);
            d.setTextSize(1);
            d.setTextColor(YELLOW, BLACK);
            d.setCursor(20, 100);
            d.printf("Connecting to\n  %s...", sel.ssid);

            // Connect
            if (sel.known) {
                // Find password in NVS
                for (size_t j = 0; j < nvsCount; j++) {
                    if (strcmp(nvsCreds[j].ssid, sel.ssid) == 0) {
                        WiFi.begin(sel.ssid, nvsCreds[j].pass);
                        break;
                    }
                }
            } else {
                WiFi.begin(sel.ssid);  // open network
            }

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                setLastConnectedSSID(sel.ssid);
                Serial.printf("[WiFi] connected to %s\n", sel.ssid);
                return true;
            } else {
                d.fillScreen(BLACK);
                d.setTextSize(1);
                d.setTextColor(RED, BLACK);
                d.setCursor(20, 100);
                d.print("Connection failed");
                delay(1500);
                drawPicker(entries, count, cursor);
            }
        }

        delay(20);
    }
}

}  // namespace stick_os
