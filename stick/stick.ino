// Stick — registry-driven launcher. stick_os enumerates AppDescriptors
// registered by app libraries via STICK_REGISTER_APP. Two-level nav:
// category picker → app list → running app. Short PWR-click pops one
// level up. OS owns the top 18 px status strip; apps never touch it.
// Portrait mode (135×240) for the launcher; apps choose their own rotation.

#include <M5StickCPlus2.h>
#include <stick_net.h>
#include <stick_os.h>

// App library includes — arduino-cli compiles libraries only when their
// headers appear in the sketch's include tree. These includes exist
// solely to trigger that discovery; the launcher uses the stick_os
// registry at runtime, not these headers directly.
#include <arcade_common.h>
#include <game_flappy.h>
#include <game_dino.h>
#include <game_scream.h>
#include <game_galaxy.h>
#include <game_balance.h>
#include <game_simon.h>
#include <game_panic.h>
#include <aipin_wifi_app.h>
#include <settings_about.h>
#include <sensor_battery.h>
#include <sensor_wifi_scan.h>
#include <sensor_imu.h>
#include <settings_wifi.h>
#include <settings_storage.h>

namespace {

using stick_os::AppCategory;
using stick_os::AppDescriptor;

enum State : uint8_t {
    ST_CATEGORIES = 0,
    ST_APP_LIST   = 1,
    ST_APP        = 2,
};

State          g_state        = ST_CATEGORIES;
uint8_t        g_catCursor    = 0;
uint8_t        g_appCursor    = 0;
AppCategory    g_openCategory = stick_os::CAT_GAME;
const AppDescriptor* g_runningApp = nullptr;
uint8_t        g_btnDrainFrames = 0;
uint32_t       g_lastStripTick = 0;

stick_os::StickStore g_osStore("stick");

const char* const kCategoryNames[stick_os::CAT_COUNT] = {
    "Games", "Apps", "Sensors", "Settings",
};

const uint16_t kCategoryColors[stick_os::CAT_COUNT] = {
    0x07E0,  // GREEN
    0x07FF,  // CYAN
    0xFFE0,  // YELLOW
    0xFD20,  // ORANGE
};

// ---------- persistence ----------
constexpr const char* kKeyLastCat = "last_cat";
char kKeyLastAppBuf[12];
const char* lastAppKey(AppCategory c) {
    snprintf(kKeyLastAppBuf, sizeof(kKeyLastAppBuf), "last_app_%u",
             static_cast<unsigned>(c));
    return kKeyLastAppBuf;
}

AppCategory loadLastCategory() {
    uint8_t v = g_osStore.getU8(kKeyLastCat, stick_os::CAT_GAME);
    if (v >= stick_os::CAT_COUNT) v = stick_os::CAT_GAME;
    return static_cast<AppCategory>(v);
}
void saveLastCategory(AppCategory c) {
    g_osStore.putU8(kKeyLastCat, static_cast<uint8_t>(c));
}
size_t loadLastAppIndex(AppCategory c) {
    return g_osStore.getU32(lastAppKey(c), 0);
}
void saveLastAppIndex(AppCategory c, size_t i) {
    g_osStore.putU32(lastAppKey(c), static_cast<uint32_t>(i));
}

// ---------- draw helpers ----------
void setPortrait() {
    StickCP2.Display.setRotation(0);  // 135×240 portrait
}

// ---------- draw: category picker (vertical list) ----------
void drawCategoryPicker() {
    setPortrait();
    auto& d = StickCP2.Display;
    const int16_t sy = stick_os::kStatusStripHeight;
    d.fillRect(0, sy, d.width(), d.height() - sy, BLACK);

    // Category tiles — vertical stack (title is in the status strip)
    const int tileH = 46;
    const int gap = 4;
    const int ox = 6;
    const int oy = sy + 4;
    const int tileW = d.width() - 12;

    for (uint8_t i = 0; i < stick_os::CAT_COUNT; i++) {
        int y = oy + i * (tileH + gap);
        const bool sel = (i == g_catCursor);
        const uint16_t accent = kCategoryColors[i];
        const uint16_t fg = sel ? accent : d.color565(90, 90, 90);
        const uint16_t bg = sel ? d.color565(15, 25, 15) : BLACK;

        d.drawRoundRect(ox, y, tileW, tileH, 4, fg);
        if (sel) d.fillRoundRect(ox + 1, y + 1, tileW - 2, tileH - 2, 4, bg);

        // Left accent bar
        if (sel) d.fillRect(ox + 1, y + 4, 3, tileH - 8, accent);

        size_t n = stick_os::appCountInCategory(static_cast<AppCategory>(i));

        d.setTextSize(2);
        d.setTextColor(sel ? WHITE : fg, bg);
        d.setCursor(ox + 10, y + 6);
        d.print(kCategoryNames[i]);

        d.setTextSize(1);
        d.setTextColor(d.color565(120, 120, 120), bg);
        d.setCursor(ox + 10, y + 28);
        d.printf("%u app%s", (unsigned)n, n == 1 ? "" : "s");
    }

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(6, d.height() - 12);
    d.print("A:open  B:next");
}

// ---------- draw: app list ----------
void drawAppList() {
    setPortrait();
    auto& d = StickCP2.Display;
    const int16_t sy = stick_os::kStatusStripHeight;
    d.fillRect(0, sy, d.width(), d.height() - sy, BLACK);

    const size_t n = stick_os::appCountInCategory(g_openCategory);

    // Category name is in the status strip; content starts immediately

    if (n == 0) {
        d.setTextSize(1);
        d.setTextColor(d.color565(120, 120, 120), BLACK);
        d.setCursor(20, sy + 40);
        d.print("No apps installed");
        d.setCursor(6, d.height() - 12);
        d.setTextColor(d.color565(80, 80, 80), BLACK);
        d.print("PWR:back");
        return;
    }

    // Scrollable app entries
    const int rowH = 38;
    const int firstRowY = sy + 4;
    const int maxVisible = 5;

    int start = static_cast<int>(g_appCursor) - 2;
    if (start < 0) start = 0;
    if (start > static_cast<int>(n) - maxVisible) start = static_cast<int>(n) - maxVisible;
    if (start < 0) start = 0;

    for (int i = 0; i < maxVisible && (start + i) < static_cast<int>(n); i++) {
        const AppDescriptor* app = stick_os::appAtInCategory(g_openCategory, start + i);
        if (app == nullptr) continue;
        const bool sel = (start + i) == static_cast<int>(g_appCursor);
        const int y = firstRowY + i * rowH;
        const uint16_t accent = kCategoryColors[g_openCategory];
        const uint16_t fg = sel ? accent : d.color565(100, 100, 100);
        const uint16_t bg = sel ? d.color565(15, 25, 15) : BLACK;

        d.drawRoundRect(4, y, d.width() - 8, rowH - 4, 4, fg);
        if (sel) d.fillRoundRect(5, y + 1, d.width() - 10, rowH - 6, 4, bg);

        // Icon
        if (app->icon) app->icon(10, y + 3, fg);

        // Name — shifted left, fits 7 chars at textSize 2
        d.setTextSize(2);
        d.setTextColor(sel ? WHITE : fg, bg);
        d.setCursor(46, y + 10);
        d.print(app->name);
    }

    // Scroll indicator
    if (n > (size_t)maxVisible) {
        int trackH = maxVisible * rowH;
        int thumbH = (trackH * maxVisible) / n;
        if (thumbH < 8) thumbH = 8;
        int thumbY = firstRowY + (trackH - thumbH) * g_appCursor / (n - 1);
        d.fillRect(d.width() - 3, firstRowY, 2, trackH, d.color565(30, 30, 30));
        d.fillRect(d.width() - 3, thumbY, 2, thumbH, d.color565(80, 80, 80));
    }

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(6, d.height() - 12);
    d.print("A:go  B:next  PWR:back");
}

// ---------- state transitions ----------
void enterCategories() {
    g_state     = ST_CATEGORIES;
    g_catCursor = static_cast<uint8_t>(loadLastCategory());
    setPortrait();
    stick_os::statusStripDrawFull();
    drawCategoryPicker();
    stick_os::statusStripTick("Stick");
    g_btnDrainFrames = 6;
    g_lastStripTick  = 0;
}

void enterAppList(AppCategory cat) {
    g_state        = ST_APP_LIST;
    g_openCategory = cat;
    size_t n = stick_os::appCountInCategory(cat);
    size_t last = loadLastAppIndex(cat);
    g_appCursor = (n == 0) ? 0 : static_cast<uint8_t>(last < n ? last : 0);
    setPortrait();
    stick_os::statusStripDrawFull();
    drawAppList();
    stick_os::statusStripTick(kCategoryNames[g_openCategory]);
    g_btnDrainFrames = 6;
    g_lastStripTick  = 0;
}

void enterApp(const AppDescriptor* app) {
    if (app == nullptr || app->runtime != stick_os::RUNTIME_NATIVE) return;
    if (app->native.init == nullptr) return;
    Serial.printf("[stick] launching %s\n", app->name);
    g_runningApp = app;

    stick_os::AppContext ctx = {
        /*contentX=*/ 0,
        /*contentY=*/ stick_os::kStatusStripHeight,
        /*contentW=*/ static_cast<int16_t>(StickCP2.Display.width()),
        /*contentH=*/ static_cast<int16_t>(StickCP2.Display.height() - stick_os::kStatusStripHeight),
        /*store=*/    nullptr,
    };
    stick_os::_setCurrentContext(ctx);

    // Don't draw the strip before the app — the app will set its own
    // rotation and the strip would end up in the wrong orientation.
    // statusStripTick() runs every 500ms in loop() AFTER the app's tick().
    app->native.init();
    g_state = ST_APP;
    g_btnDrainFrames = 8;
}

// ---------- tick handlers ----------
void tickCategories() {
    if (StickCP2.BtnB.wasPressed()) {
        g_catCursor = (g_catCursor + 1) % stick_os::CAT_COUNT;
        drawCategoryPicker();
        stick_os::statusStripTick("Stick");
    }
    if (StickCP2.BtnA.wasPressed()) {
        AppCategory cat = static_cast<AppCategory>(g_catCursor);
        saveLastCategory(cat);
        enterAppList(cat);
    }
}

void tickAppList() {
    size_t n = stick_os::appCountInCategory(g_openCategory);

    if (StickCP2.BtnB.wasPressed() && n > 0) {
        g_appCursor = (g_appCursor + 1) % n;
        drawAppList();
        stick_os::statusStripTick(kCategoryNames[g_openCategory]);
    }
    if (StickCP2.BtnA.wasPressed() && n > 0) {
        saveLastAppIndex(g_openCategory, g_appCursor);
        enterApp(stick_os::appAtInCategory(g_openCategory, g_appCursor));
    }
}

}  // namespace

void setup() {
    StickCP2.begin();
    Serial.begin(115200);
    delay(50);
    Serial.println("stick booted");
    Serial.printf("[stick_os] %u apps registered\n",
                  (unsigned)stick_os::appCount());
    for (size_t i = 0; i < stick_os::appCount(); i++) {
        auto* d = stick_os::appAt(i);
        Serial.printf("  [%u] %s (cat=%u)\n", (unsigned)i, d->name, d->category);
    }

    StickNet::startAsync();
    stick_os::statusStripInit();

    enterCategories();
}

void loop() {
    StickCP2.update();

    if (g_btnDrainFrames > 0) { g_btnDrainFrames--; delay(10); return; }

    // PWR-click: pop one level up.
    if (M5.BtnPWR.wasClicked()) {
        switch (g_state) {
            case ST_APP:
                Serial.println("[stick] pwr: app -> list");
                g_runningApp = nullptr;
                enterAppList(g_openCategory);
                return;
            case ST_APP_LIST:
                Serial.println("[stick] pwr: list -> categories");
                enterCategories();
                return;
            case ST_CATEGORIES:
            default:
                break;
        }
    }

    // Running app: forward the tick. Check exit flag after.
    if (g_state == ST_APP) {
        if (g_runningApp && g_runningApp->native.tick) {
            g_runningApp->native.tick();
        }
        // If the app's init() or tick() detected a PWR press via
        // stick_os::checkAppExit(), transition back to the app list.
        if (stick_os::wasExitRequested()) {
            stick_os::clearExitRequest();
            Serial.println("[stick] exit requested -> list");
            g_runningApp = nullptr;
            enterAppList(g_openCategory);
        }
        return;
    }

    // Launcher: handle input then repaint strip periodically.
    if (g_state == ST_CATEGORIES) tickCategories();
    else                          tickAppList();

    const uint32_t now = millis();
    if (now - g_lastStripTick > 500) {
        const char* label = (g_state == ST_CATEGORIES)
            ? "Stick" : kCategoryNames[g_openCategory];
        stick_os::statusStripTick(label);
        g_lastStripTick = now;
    }
    delay(20);
}
