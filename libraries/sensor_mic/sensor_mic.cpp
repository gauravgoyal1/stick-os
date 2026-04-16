#include <M5StickCPlus2.h>
#include <stick_os.h>
#include <math.h>
#include "sensor_mic.h"

namespace SensorMic {

constexpr int kHistLen = 45;
static int g_hist[kHistLen] = {0};
static int g_histIdx = 0;
static int g_peak = 0;
static unsigned long g_peakTime = 0;

int readMicLevel() {
    static int16_t soundData[256];
    memset(soundData, 0, sizeof(soundData));

    if (!StickCP2.Mic.isEnabled()) {
        StickCP2.Mic.begin();
        delay(10);
    }

    StickCP2.Mic.record(soundData, 256, 16000);

    unsigned long startTime = millis();
    while (StickCP2.Mic.isRecording()) {
        if (millis() - startTime > 100) break;
        delay(1);
    }

    int32_t sum = 0;
    int16_t maxVal = 0;
    for (int i = 0; i < 256; i++) {
        int16_t v = abs(soundData[i]);
        sum += v;
        if (v > maxVal) maxVal = v;
    }
    return sum / 256;
}

void drawBar(int level) {
    auto& d = StickCP2.Display;
    const int barX = 10, barY = 40, barW = d.width() - 20, barH = 30;
    int fillW = constrain(level * barW / 300, 0, barW);

    uint16_t fillColor;
    if (level > 200)     fillColor = RED;
    else if (level > 80) fillColor = YELLOW;
    else                 fillColor = GREEN;

    d.fillRect(barX, barY, fillW, barH, fillColor);
    d.fillRect(barX + fillW, barY, barW - fillW, barH, d.color565(20, 20, 20));
    d.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, d.color565(60, 60, 60));

    // Peak hold line
    unsigned long now = millis();
    if (level > g_peak || now - g_peakTime > 2000) {
        g_peak = level;
        g_peakTime = now;
    }
    // Decay peak slowly
    if (now - g_peakTime > 800 && g_peak > 0) {
        g_peak -= 2;
        if (g_peak < 0) g_peak = 0;
    }
    int peakX = barX + constrain(g_peak * barW / 300, 0, barW - 1);
    d.drawFastVLine(peakX, barY, barH, RED);
}

void drawDB(int level) {
    auto& d = StickCP2.Display;
    float db = (level > 0) ? 20.0f * log10f((float)level) : 0.0f;
    d.setTextSize(3);
    d.setTextColor(WHITE, BLACK);
    d.setCursor(14, 80);
    d.printf("%5.1f", db);
    d.setTextSize(1);
    d.setCursor(108, 90);
    d.print("dB");
}

void drawWaveform() {
    auto& d = StickCP2.Display;
    const int wx = 4, wy = 130, ww = d.width() - 8, wh = 80;
    const int centerY = wy + wh / 2;

    d.fillRect(wx, wy, ww, wh, BLACK);
    d.drawFastHLine(wx, centerY, ww, d.color565(30, 30, 30));
    d.drawRect(wx, wy, ww, wh, d.color565(30, 30, 30));

    int barW = ww / kHistLen;
    if (barW < 1) barW = 1;

    for (int i = 0; i < kHistLen; i++) {
        int idx = (g_histIdx + i) % kHistLen;
        int val = g_hist[idx];
        int h = constrain(val * (wh / 2) / 200, 0, wh / 2 - 1);

        uint16_t color;
        if (val > 200)     color = RED;
        else if (val > 80) color = YELLOW;
        else               color = d.color565(0, 180, 0);

        if (h > 0) {
            d.fillRect(wx + i * barW, centerY - h, barW - 1, h, color);
            d.fillRect(wx + i * barW, centerY, barW - 1, h, color);
        }
    }
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);

    g_histIdx = 0;
    g_peak = 0;
    g_peakTime = 0;
    memset(g_hist, 0, sizeof(g_hist));

    // Title
    d.setTextSize(2);
    d.setTextColor(GREEN, BLACK);
    d.setCursor(8, 8);
    d.print("Mic Meter");
    d.drawFastHLine(0, 30, d.width(), d.color565(40, 40, 40));

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        int level = readMicLevel();

        g_hist[g_histIdx] = level;
        g_histIdx = (g_histIdx + 1) % kHistLen;

        drawBar(level);
        drawDB(level);
        drawWaveform();

        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Three vertical bars at different heights
    d.fillRect(x + 6,  y + 14, 4, 10, color);
    d.fillRect(x + 12, y + 6,  4, 18, color);
    d.fillRect(x + 18, y + 10, 4, 14, color);
}

}  // namespace SensorMic

static const stick_os::AppDescriptor kDesc = {
    "mic", "Mic", "1.0.0",
    stick_os::CAT_SENSOR, stick_os::APP_NEEDS_MIC,
    &SensorMic::icon, stick_os::RUNTIME_NATIVE,
    { &SensorMic::init, &SensorMic::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
