#include "app_context.h"

#include <M5StickCPlus2.h>

namespace stick_os {

namespace {
AppContext g_ctx = {
    /*contentX=*/0, /*contentY=*/18,
    /*contentW=*/240, /*contentH=*/135 - 18,
    /*store=*/nullptr,
};
volatile bool g_exitRequested = false;
}  // namespace

const AppContext& currentContext() { return g_ctx; }
void _setCurrentContext(const AppContext& ctx) { g_ctx = ctx; }

bool checkAppExit() {
    if (M5.BtnPWR.wasClicked()) { g_exitRequested = true; return true; }
    return false;
}
bool wasExitRequested() { return g_exitRequested; }
void clearExitRequest() { g_exitRequested = false; }

}  // namespace stick_os
