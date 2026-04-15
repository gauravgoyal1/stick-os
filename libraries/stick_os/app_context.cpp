#include "app_context.h"

namespace stick_os {

namespace {
AppContext g_ctx = {
    /*contentX=*/0, /*contentY=*/18,
    /*contentW=*/240, /*contentH=*/135 - 18,
    /*store=*/nullptr,
};
}  // namespace

const AppContext& currentContext() { return g_ctx; }
void _setCurrentContext(const AppContext& ctx) { g_ctx = ctx; }

}  // namespace stick_os
