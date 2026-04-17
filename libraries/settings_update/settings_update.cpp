#include <M5StickCPlus2.h>
#include <stick_net.h>
#include <stick_os.h>
#include "settings_update.h"

namespace SettingsUpdate {

// Baked-in firmware version. Bump when cutting a release.
static const char* kFirmwareVersion = "1.0.0";

enum Phase : uint8_t {
    PH_IDLE = 0,
    PH_CHECKING,
    PH_NO_UPDATE,
    PH_FOUND,
    PH_DOWNLOADING,
    PH_DONE,
    PH_ERROR,
};

static Phase g_phase  = PH_IDLE;
static stick_os::OtaInfo g_info;
static char g_error[48];
static uint32_t g_progDone = 0;
static uint32_t g_progTotal = 0;
static uint32_t g_lastProgDraw = 0;

static void drawHeader() {
    auto& d = StickCP2.Display;
    d.fillScreen(BLACK);
    d.setTextSize(2);
    d.setTextColor(d.color565(255, 165, 0), BLACK);
    d.setCursor(8, 8);
    d.print("Update");
    d.drawFastHLine(0, 36, d.width(), d.color565(40, 40, 40));
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(8, 46);
    d.printf("Current: %s", kFirmwareVersion);
}

static void drawFooter(const char* hint) {
    auto& d = StickCP2.Display;
    d.fillRect(0, d.height() - 16, d.width(), 16, BLACK);
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(6, d.height() - 12);
    d.print(hint);
}

static void drawStatusArea(const char* line1, const char* line2 = nullptr,
                            uint16_t color = WHITE) {
    auto& d = StickCP2.Display;
    d.fillRect(0, 70, d.width(), d.height() - 70 - 18, BLACK);
    d.setTextSize(1);
    d.setTextColor(color, BLACK);
    d.setCursor(8, 80);
    d.print(line1);
    if (line2) {
        d.setCursor(8, 96);
        d.print(line2);
    }
}

static void drawProgressBar(uint32_t done, uint32_t total) {
    auto& d = StickCP2.Display;
    const int x = 8, y = 150;
    const int w = d.width() - 16, h = 14;
    d.drawRect(x, y, w, h, d.color565(80, 80, 80));
    int fill = (total > 0) ? (int)((uint64_t)done * (w - 2) / total) : 0;
    d.fillRect(x + 1, y + 1, fill, h - 2, d.color565(0, 160, 255));
    d.fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2,
               d.color565(20, 20, 20));

    char label[32];
    snprintf(label, sizeof(label), "%u / %u KB",
             (unsigned)(done / 1024), (unsigned)(total / 1024));
    d.fillRect(x, y + h + 4, w, 12, BLACK);
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 140, 140), BLACK);
    d.setCursor(x, y + h + 4);
    d.print(label);
}

static void otaProgress(uint32_t done, uint32_t total) {
    g_progDone = done;
    g_progTotal = total;
    const uint32_t now = millis();
    if (now - g_lastProgDraw < 100) return;  // throttle draws
    g_lastProgDraw = now;
    drawProgressBar(done, total);
}

static bool versionIsNewer(const char* remote, const char* current) {
    // Naive string compare is enough for "1.2.3" style versions, but
    // do a simple dotted-int compare to handle 1.10 > 1.9.
    int r[3] = {0, 0, 0};
    int c[3] = {0, 0, 0};
    sscanf(remote, "%d.%d.%d", &r[0], &r[1], &r[2]);
    sscanf(current, "%d.%d.%d", &c[0], &c[1], &c[2]);
    for (int i = 0; i < 3; i++) {
        if (r[i] > c[i]) return true;
        if (r[i] < c[i]) return false;
    }
    return false;
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);

    g_phase = PH_IDLE;
    drawHeader();
    drawStatusArea("Press A to check", "for updates");
    drawFooter("A: check  PWR: back");

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        if (StickCP2.BtnA.wasPressed()) {
            switch (g_phase) {
                case PH_IDLE:
                case PH_NO_UPDATE:
                case PH_DONE:
                case PH_ERROR: {
                    if (!StickNet::isConnected()) {
                        drawStatusArea("WiFi offline", "cannot check", RED);
                        g_phase = PH_ERROR;
                        break;
                    }
                    g_phase = PH_CHECKING;
                    drawStatusArea("Checking...", nullptr, YELLOW);
                    if (stick_os::otaCheckForUpdate(&g_info)) {
                        if (g_info.version[0] == 0 ||
                            !versionIsNewer(g_info.version, kFirmwareVersion)) {
                            drawStatusArea("Up to date", nullptr, GREEN);
                            g_phase = PH_NO_UPDATE;
                        } else {
                            char line[40];
                            snprintf(line, sizeof(line), "Found: %s",
                                     g_info.version);
                            drawStatusArea(line, "A: install", CYAN);
                            drawFooter("A: install  PWR: back");
                            g_phase = PH_FOUND;
                        }
                    } else {
                        drawStatusArea("Check failed", "server unreachable", RED);
                        g_phase = PH_ERROR;
                    }
                    break;
                }
                case PH_FOUND: {
                    g_phase = PH_DOWNLOADING;
                    g_progDone = 0;
                    g_progTotal = g_info.size;
                    drawStatusArea("Downloading...", nullptr, YELLOW);
                    drawProgressBar(0, g_info.size);
                    drawFooter("PWR: cancel");
                    bool ok = stick_os::otaDownloadAndApply(&g_info, otaProgress);
                    if (ok) {
                        drawStatusArea("Update installed", "rebooting...", GREEN);
                        delay(1500);
                        ESP.restart();
                    } else {
                        drawStatusArea("Install failed",
                                       "check serial log", RED);
                        drawFooter("A: retry  PWR: back");
                        g_phase = PH_ERROR;
                    }
                    break;
                }
                default: break;
            }
        }
        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Down arrow into a tray — "download"
    d.drawLine(x + 14, y + 4, x + 14, y + 16, color);
    d.drawLine(x + 14, y + 16, x + 9,  y + 11, color);
    d.drawLine(x + 14, y + 16, x + 19, y + 11, color);
    d.drawFastHLine(x + 4, y + 22, 22, color);
    d.drawLine(x + 4, y + 22, x + 4, y + 26, color);
    d.drawLine(x + 25, y + 22, x + 25, y + 26, color);
    d.drawFastHLine(x + 4, y + 26, 22, color);
}

}  // namespace SettingsUpdate

static const stick_os::AppDescriptor kDesc = {
    "update", "Update", "1.0.0",
    stick_os::CAT_SETTINGS, stick_os::APP_SYSTEM_LOCKED | stick_os::APP_NEEDS_NET,
    &SettingsUpdate::icon, stick_os::RUNTIME_NATIVE,
    { &SettingsUpdate::init, &SettingsUpdate::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
