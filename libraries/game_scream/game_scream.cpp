#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_scream.h"

namespace GameScream {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);
    StickCP2.Mic.begin();

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    float birdY = SCREEN_H_LANDSCAPE / 2;
    int score = 0;
    int wallX = SCREEN_W_LANDSCAPE;
    int gapY = 50;

    // --- UPDATED SENSITIVITY ---
    // Lowered threshold to 250 for easier play
    int NOISE_THRESHOLD = 250;

    StickCP2.Display.setCursor(30, 50); StickCP2.Display.setTextColor(C_YELLOW); StickCP2.Display.print("SCREAM TO FLY!");
    delay(1500);
    StickCP2.Display.fillScreen(C_BLACK);

    bool running = true;
    while(running) {
        if (ArcadeCommon::updateAndCheckExit()) return;

        int noise = getNoiseLevel();
        // Visual Bar
        int barLen = map(noise, 0, 1000, 0, SCREEN_W_LANDSCAPE);
        StickCP2.Display.drawFastHLine(0, 134, SCREEN_W_LANDSCAPE, C_BLACK);
        StickCP2.Display.drawFastHLine(0, 134, barLen, (noise > NOISE_THRESHOLD ? C_GREEN : C_RED));

        StickCP2.Display.fillRect(40, (int)birdY, 15, 15, C_BLACK);
        StickCP2.Display.fillRect(wallX, 0, 20, 135, C_BLACK);

        if(noise > NOISE_THRESHOLD) birdY -= 6; // Flap Strength
        else birdY += 2;                        // Fall Speed (Slower than before)

        // --- UPDATED SPEED ---
        // Wall moves slower (2 pixels per frame instead of 4)
        wallX -= 2;
        if(wallX < -20) {
            wallX = SCREEN_W_LANDSCAPE;
            gapY = random(10, 80);
            score++;
            StickCP2.Speaker.tone(4000, 50);
        }

        StickCP2.Display.fillRect(40, (int)birdY, 15, 15, C_CYAN);
        StickCP2.Display.fillRect(wallX, 0, 20, gapY, C_GREEN);
        StickCP2.Display.fillRect(wallX, gapY + 50, 20, 135, C_GREEN);

        StickCP2.Display.setCursor(5, 5); StickCP2.Display.setTextColor(C_WHITE, C_BLACK); StickCP2.Display.print(score);

        if(birdY < 0 || birdY > 135) running = false;
        if(40 + 15 > wallX && 40 < wallX + 20) {
            if(birdY < gapY || birdY + 15 > gapY + 50) running = false;
        }

        if(StickCP2.BtnA.wasPressed()) running = false;

        // Added slight delay to slow down the whole game
        delay(30);
    }

    int highScore = loadHighScore("scream");
    if(saveHighScoreIfBetter("scream", score)) highScore = score;

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // VU meter: three bars at increasing heights
    d.fillRect(x + 6,  y + 18, 6, 10, color);
    d.fillRect(x + 15, y + 12, 6, 16, color);
    d.fillRect(x + 24, y + 4,  6, 24, color);
}

}  // namespace GameScream

static const stick_os::AppDescriptor kDesc = {
    "scream", "Scream", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NEEDS_MIC,
    &GameScream::icon, stick_os::RUNTIME_NATIVE,
    { &GameScream::init, &GameScream::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
