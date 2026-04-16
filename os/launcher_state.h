#pragma once

// Shared launcher state, constants, and function declarations.
// Split from os.ino for readability — all symbols live in the
// `launcher` namespace to avoid polluting the global scope.

#include <M5StickCPlus2.h>
#include <stick_os.h>

namespace launcher {

// ---------- state machine ----------
enum State : uint8_t {
    ST_CATEGORIES = 0,
    ST_APP_LIST   = 1,
    ST_APP        = 2,
};

extern State          g_state;
extern uint8_t        g_catCursor;
extern uint8_t        g_appCursor;
extern stick_os::AppCategory g_openCategory;
extern const stick_os::AppDescriptor* g_runningApp;
extern uint8_t        g_btnDrainFrames;
extern uint32_t       g_lastStripTick;

// ---------- constants ----------
extern const char* const kCategoryNames[stick_os::CAT_COUNT];
extern const uint16_t    kCategoryColors[stick_os::CAT_COUNT];

// ---------- draw ----------
void setPortrait();
void drawCategoryPicker();
void drawAppList();

// ---------- navigation / state transitions ----------
void enterCategories();
void enterAppList(stick_os::AppCategory cat);
void enterApp(const stick_os::AppDescriptor* app);

// ---------- tick ----------
void tickCategories();
void tickAppList();

// ---------- serial ----------
void processSerialCommand();

}  // namespace launcher
