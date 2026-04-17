// Launcher navigation: persistence helpers, state transitions, tick handlers.

#include "launcher_state.h"

namespace launcher {

// ---------- globals ----------
State          g_state        = ST_CATEGORIES;
uint8_t        g_catCursor    = 0;
uint8_t        g_appCursor    = 0;
stick_os::AppCategory g_openCategory = stick_os::CAT_GAME;
const stick_os::AppDescriptor* g_runningApp = nullptr;
uint8_t        g_btnDrainFrames = 0;
uint32_t       g_lastStripTick = 0;

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
static stick_os::StickStore g_osStore("stick");
static constexpr const char* kKeyLastCat = "last_cat";
static char kKeyLastAppBuf[12];

static const char* lastAppKey(stick_os::AppCategory c) {
    snprintf(kKeyLastAppBuf, sizeof(kKeyLastAppBuf), "last_app_%u",
             static_cast<unsigned>(c));
    return kKeyLastAppBuf;
}

static stick_os::AppCategory loadLastCategory() {
    uint8_t v = g_osStore.getU8(kKeyLastCat, stick_os::CAT_GAME);
    if (v >= stick_os::CAT_COUNT) v = stick_os::CAT_GAME;
    return static_cast<stick_os::AppCategory>(v);
}

static void saveLastCategory(stick_os::AppCategory c) {
    g_osStore.putU8(kKeyLastCat, static_cast<uint8_t>(c));
}

static size_t loadLastAppIndex(stick_os::AppCategory c) {
    return g_osStore.getU32(lastAppKey(c), 0);
}

static void saveLastAppIndex(stick_os::AppCategory c, size_t i) {
    g_osStore.putU32(lastAppKey(c), static_cast<uint32_t>(i));
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

void enterAppList(stick_os::AppCategory cat) {
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

void enterApp(const stick_os::AppDescriptor* app) {
    if (app == nullptr) return;

    if (app->runtime == stick_os::RUNTIME_MPY) {
        if (app->script.path == nullptr) {
            Serial.println("[stick] scripted app has no script.path");
            return;
        }
        Serial.printf("[stick] launching (mpy) %s from %s\n",
                      app->name, app->script.path);
        g_runningApp = app;

        stick_os::AppContext ctx = {
            /*contentX=*/ 0,
            /*contentY=*/ stick_os::kStatusStripHeight,
            /*contentW=*/ static_cast<int16_t>(StickCP2.Display.width()),
            /*contentH=*/ static_cast<int16_t>(
                StickCP2.Display.height() - stick_os::kStatusStripHeight),
            /*store=*/    nullptr,
        };
        stick_os::_setCurrentContext(ctx);

        setPortrait();
        stick_os::statusStripDrawFull();
        stick_os::statusStripTick(app->name);
        stick_os::logHeap(app->name);

        // Blocking — returns when the script exits (via PWR) or finishes.
        stick_os::scriptRunFile(app->script.path);

        stick_os::logHeap("mpy-exit");
        stick_os::clearExitRequest();
        g_runningApp = nullptr;
        g_btnDrainFrames = 6;
        // Fall through to the app list — caller (loop()) routes us back.
        enterAppList(g_openCategory);
        return;
    }

    if (app->runtime != stick_os::RUNTIME_NATIVE) return;
    if (app->native.init == nullptr) return;
    Serial.printf("[stick] launching %s\n", app->name);
    g_runningApp = app;

    stick_os::AppContext ctx = {
        /*contentX=*/ 0,
        /*contentY=*/ stick_os::kStatusStripHeight,
        /*contentW=*/ static_cast<int16_t>(StickCP2.Display.width()),
        /*contentH=*/ static_cast<int16_t>(
            StickCP2.Display.height() - stick_os::kStatusStripHeight),
        /*store=*/    nullptr,
    };
    stick_os::_setCurrentContext(ctx);

    stick_os::logHeap(app->name);
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
        stick_os::AppCategory cat =
            static_cast<stick_os::AppCategory>(g_catCursor);
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

}  // namespace launcher
