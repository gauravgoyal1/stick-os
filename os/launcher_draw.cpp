// Draw routines for the launcher: category picker and app list.

#include "launcher_state.h"

namespace launcher {

void setPortrait() {
    StickCP2.Display.setRotation(0);  // 135x240 portrait
}

// ---------- category picker (vertical list) ----------
void drawCategoryPicker() {
    setPortrait();
    auto& d = StickCP2.Display;
    const int16_t sy = stick_os::kStatusStripHeight;
    d.fillRect(0, sy, d.width(), d.height() - sy, BLACK);

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

        size_t n = stick_os::appCountInCategory(
            static_cast<stick_os::AppCategory>(i));

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

// ---------- app list ----------
void drawAppList() {
    setPortrait();
    auto& d = StickCP2.Display;
    const int16_t sy = stick_os::kStatusStripHeight;
    d.fillRect(0, sy, d.width(), d.height() - sy, BLACK);

    const size_t n = stick_os::appCountInCategory(g_openCategory);

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
    if (start > static_cast<int>(n) - maxVisible)
        start = static_cast<int>(n) - maxVisible;
    if (start < 0) start = 0;

    for (int i = 0; i < maxVisible && (start + i) < static_cast<int>(n);
         i++) {
        const stick_os::AppDescriptor* app =
            stick_os::appAtInCategory(g_openCategory, start + i);
        if (app == nullptr) continue;
        const bool sel = (start + i) == static_cast<int>(g_appCursor);
        const int y = firstRowY + i * rowH;
        const uint16_t accent = kCategoryColors[g_openCategory];
        const uint16_t fg = sel ? accent : d.color565(100, 100, 100);
        const uint16_t bg = sel ? d.color565(15, 25, 15) : BLACK;

        d.drawRoundRect(4, y, d.width() - 8, rowH - 4, 4, fg);
        if (sel)
            d.fillRoundRect(5, y + 1, d.width() - 10, rowH - 6, 4, bg);

        // Icon
        if (app->icon) app->icon(10, y + 3, fg);

        // Name — 7 chars max at textSize 2 (89px available from x=46)
        d.setTextSize(2);
        d.setTextColor(sel ? WHITE : fg, bg);
        d.setCursor(46, y + 10);
        char truncName[8];
        strncpy(truncName, app->name, 7);
        truncName[7] = '\0';
        d.print(truncName);
    }

    // Scroll indicator
    if (n > (size_t)maxVisible) {
        int trackH = maxVisible * rowH;
        int thumbH = (trackH * maxVisible) / n;
        if (thumbH < 8) thumbH = 8;
        int thumbY =
            firstRowY + (trackH - thumbH) * g_appCursor / (n - 1);
        d.fillRect(d.width() - 3, firstRowY, 2, trackH,
                   d.color565(30, 30, 30));
        d.fillRect(d.width() - 3, thumbY, 2, thumbH,
                   d.color565(80, 80, 80));
    }

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(6, d.height() - 12);
    d.print("A:go  B:next  PWR:back");
}

}  // namespace launcher
