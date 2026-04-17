#include "app_icon.h"

#include <M5StickCPlus2.h>
#include <ctype.h>

#include "app_descriptor.h"

namespace stick_os {

void drawLetterTile(int x, int y, const char* name, uint16_t color) {
    auto& d = StickCP2.Display;
    d.drawRoundRect(x + 2, y + 2, 28, 28, 5, color);
    char c = (name && *name) ? (char)toupper((unsigned char)*name) : '?';
    d.setTextSize(2);
    d.setTextColor(color, BLACK);
    // Size-2 glyph is ~12x16 px; center inside the 28x28 tile.
    d.setCursor(x + 10, y + 9);
    d.print(c);
}

void drawAppIconOrFallback(int x, int y, const AppDescriptor* app,
                            uint16_t color) {
    if (app && app->icon) {
        app->icon(x, y, color);
        return;
    }
    drawLetterTile(x, y, app ? app->name : nullptr, color);
}

}  // namespace stick_os
